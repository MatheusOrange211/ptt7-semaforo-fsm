#!/usr/bin/env python3
"""
analyze.py - Analise dos dados de bancada (Q2: latencia; Q3: fallback e recuperacao).

Entrada: logs seriais brutos capturados em data/raw/ (linhas LAT, FAULT, REC, #DESCARTE).
Descartes manuais: data/descartes.csv  (arquivo,tipo,sessao,seq,motivo)  - so erro comprovado
de instrumentacao ou ensaio fora da configuracao (protocolo 6.3). Respostas lentas e falhas
do sistema NUNCA sao descartadas.

Uso:
    python analysis/analyze.py                    # todos os arquivos de data/raw/
    python analysis/analyze.py data/raw/s1.log    # arquivos especificos
    python analysis/analyze.py --plataforma sim   # inclui dados do simulador (Plano B)

Saida (analysis/out/): resumo.md, latencias.csv, falhas.csv, recuperacao.csv e graficos PNG
(se matplotlib estiver instalado).
"""
import argparse
import csv
import glob
import os
import statistics as st
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "analysis", "out")
TIPOS = {1: "perda sinal botao 1", 2: "perda sinal botao 2", 3: "perda simultanea em VP", 4: "falha de temporizacao"}
MIN_LAT_VALIDAS, MIN_SESSOES, MIN_INJECOES = 30, 3, 50


def describe(xs):
    if not xs:
        return None
    xs = sorted(xs)
    q = st.quantiles(xs, n=4, method="inclusive") if len(xs) >= 2 else [xs[0]] * 3
    return {"n": len(xs), "media": st.mean(xs), "mediana": st.median(xs),
            "dp": st.stdev(xs) if len(xs) >= 2 else 0.0, "q1": q[0], "q3": q[2], "iqr": q[2] - q[0],
            "min": xs[0], "max": xs[-1]}


def fmt(d, nd=1):
    if not d:
        return "| - | - | - | - | - | - | - | - |"
    return "| {n} | {m:.{p}f} | {md:.{p}f} | {dp:.{p}f} | {iqr:.{p}f} ({q1:.{p}f}-{q3:.{p}f}) | {mi:.{p}f} | {ma:.{p}f} |".format(
        n=d["n"], m=d["media"], md=d["mediana"], dp=d["dp"], iqr=d["iqr"], q1=d["q1"], q3=d["q3"],
        mi=d["min"], ma=d["max"], p=nd)


def load_discards():
    path = os.path.join(ROOT, "data", "descartes.csv")
    keys = {}
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            for r in csv.DictReader(f):
                try:
                    if not r.get("arquivo") or not r.get("tipo"):
                        continue
                    keys[(os.path.basename(r["arquivo"].strip()), r["tipo"].strip().upper(), int(r["sessao"]), int(r["seq"]))] = (r.get("motivo") or "").strip()
                except (KeyError, ValueError):
                    continue
    return keys


def parse(files, plataformas):
    lat, faults, recs, desc = [], [], [], []
    discards = load_discards()
    for path in files:
        base = os.path.basename(path)
        with open(path, encoding="utf-8", errors="replace") as f:
            em_lat = False          # True apenas entre "#INICIO lat" e "#FIM lat" (experimento controlado de Q2)
            for ln, line in enumerate(f, 1):
                line = line.strip()
                if line.startswith("#INICIO lat"):
                    em_lat = True
                elif line.startswith("#FIM lat") or line.startswith("# captura iniciada"):
                    em_lat = False
                p = line.split(",")
                try:
                    if p[0] == "LAT" and len(p) >= 10:
                        r = {"arquivo": base, "linha": ln, "sessao": int(p[1]), "seq": int(p[2]), "aquecimento": int(p[3]),
                             "botao": int(p[4]), "origem": p[5], "transicao": p[6], "t_isr_us": int(p[7]),
                             "lat_decisao_us": int(p[8]), "lat_saida_us": int(p[9])}
                        # LAT fora de um experimento "lat" = acionamento de botao nao controlado (incidental)
                        r["controlado"] = int(em_lat)
                        k = (base, "LAT", r["sessao"], r["seq"])
                        r["descartado"] = discards.get(k, "")
                        lat.append(r)
                    elif p[0] == "FAULT" and len(p) >= 10:
                        if p[9] not in plataformas:
                            continue
                        r = {"arquivo": base, "linha": ln, "sessao": int(p[1]), "seq": int(p[2]), "tipo": int(p[3]),
                             "estado_injecao": p[4], "t_inj_us": int(p[5]), "conv_ms": float(p[6]),
                             "sucesso": int(p[7]), "verde_apos_falha": int(p[8]), "plataforma": p[9]}
                        r["descartado"] = discards.get((base, "FAULT", r["sessao"], r["seq"]), "")
                        faults.append(r)
                    elif p[0] == "REC" and len(p) >= 7:
                        recs.append({"arquivo": base, "sessao": int(p[1]), "seq": int(p[2]), "tipo": int(p[3]),
                                     "sucesso": int(p[4]), "ms_ate_recovery": float(p[5]), "ms_ate_verde": float(p[6])})
                    elif p[0] == "#DESCARTE":
                        desc.append({"arquivo": base, "linha": ln, "registro": line})
                except (ValueError, IndexError):
                    continue
    return lat, faults, recs, desc


def write_csv(path, rows):
    if not rows:
        return
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)


def plots(lat_valid, faults_valid):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return "matplotlib nao instalado: graficos nao gerados (pip install matplotlib)."
    made = []
    by_tr = defaultdict(list)
    for r in lat_valid:
        by_tr[f"{r['origem']}\n{r['transicao']}"].append(r["lat_saida_us"])
    if by_tr:
        fig, ax = plt.subplots(figsize=(7, 4))
        ax.boxplot(list(by_tr.values()))
        ax.set_xticks(range(1, len(by_tr) + 1))
        ax.set_xticklabels(list(by_tr.keys()))
        ax.set_ylabel("LREC (us): ISR -> GPIO")
        ax.set_title("Latencia interna de software por transicao")
        fig.tight_layout()
        fig.savefig(os.path.join(OUT, "latencia_boxplot.png"), dpi=150)
        made.append("latencia_boxplot.png")
    by_t = defaultdict(list)
    for r in faults_valid:
        if r["conv_ms"] >= 0:
            by_t[TIPOS.get(r["tipo"], r["tipo"])].append(r["conv_ms"])
    if by_t:
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.boxplot(list(by_t.values()))
        ax.set_xticks(range(1, len(by_t) + 1))
        ax.set_xticklabels(list(by_t.keys()), fontsize=8)
        ax.set_ylabel("tempo de convergencia (ms)")
        ax.axhline(1000, ls="--", lw=1)
        ax.set_title("Convergencia ao estado seguro (FALHA) por tipo de falha")
        fig.tight_layout()
        fig.savefig(os.path.join(OUT, "convergencia_boxplot.png"), dpi=150)
        made.append("convergencia_boxplot.png")
    return "Graficos: " + ", ".join(made) if made else "sem dados para graficos."


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("arquivos", nargs="*")
    ap.add_argument("--plataforma", action="append", default=None,
                    help="plataformas de FAULT a incluir (padrao: esp32). Use --plataforma sim para o Plano B.")
    a = ap.parse_args()
    plataformas = set(a.plataforma or ["esp32"])
    files = a.arquivos or sorted(glob.glob(os.path.join(ROOT, "data", "raw", "*.log")) +
                                 glob.glob(os.path.join(ROOT, "data", "raw", "*.txt")))
    if not files:
        print("Nenhum log em data/raw/. Capture com tools/capture_serial.py (ver MANUAL_EXECUCAO.md).")
        return 1
    os.makedirs(OUT, exist_ok=True)
    lat, faults, recs, desc = parse(files, plataformas)

    # Valida para o protocolo de Q2: dentro do experimento "lat", fora do aquecimento e nao descartada.
    lat_valid = [r for r in lat if r["controlado"] and not r["aquecimento"] and not r["descartado"]]
    lat_incid = [r for r in lat if not r["controlado"]]
    faults_valid = [r for r in faults if not r["descartado"]]
    write_csv(os.path.join(OUT, "latencias.csv"), lat)
    write_csv(os.path.join(OUT, "falhas.csv"), faults)
    write_csv(os.path.join(OUT, "recuperacao.csv"), recs)

    L = ["# Resultados de bancada - PTT7", "", f"Arquivos: {', '.join(os.path.basename(x) for x in files)}",
         f"Plataformas consideradas nas falhas: {', '.join(sorted(plataformas))}", ""]

    # ---------------- Q2
    L += ["## Q2 - Latencia interna de software (LREC = ISR -> GPIO atualizado), em us", "",
          f"Registros LAT: {len(lat)} | fora de experimento 'lat' (incidentais, nao contam): {len(lat_incid)} | "
          f"aquecimento excluido: {sum(r['aquecimento'] for r in lat if r['controlado'])} | "
          f"descartados (data/descartes.csv): {sum(1 for r in lat if r['descartado'])} | validos: {len(lat_valid)}", "",
          "| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |", "|---|---|---|---|---|---|---|---|"]
    groups = defaultdict(list)
    for r in lat_valid:
        groups[(r["origem"], r["transicao"])].append(r)
    for (orig, tr), rows in sorted(groups.items()):
        L.append(f"| {orig} {tr} " + fmt(describe([r["lat_saida_us"] for r in rows])))
    L += ["", "Latencia ate a decisao da FSM (antes de escrever os GPIOs), em us:", "",
          "| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |", "|---|---|---|---|---|---|---|---|"]
    for (orig, tr), rows in sorted(groups.items()):
        L.append(f"| {orig} {tr} " + fmt(describe([r["lat_decisao_us"] for r in rows])))
    L += ["", "Por sessao (transicao critica VERDE_VEICULO T4 -> ATENCAO_VEICULO):", "",
          "| Sessao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |", "|---|---|---|---|---|---|---|---|"]
    t4 = [r for r in lat_valid if r["transicao"] == "T4"]
    for s in sorted({r["sessao"] for r in t4}):
        L.append(f"| {s} " + fmt(describe([r["lat_saida_us"] for r in t4 if r["sessao"] == s])))
    sess = len({r["sessao"] for r in t4})
    ok_lat = len(t4) >= MIN_LAT_VALIDAS and sess >= MIN_SESSOES
    # Por janela de captura (1 arquivo = 1 janela). Regra adicionada em 26/11/2026, DEPOIS de ver os dados
    # do reteste (todas as capturas com o mesmo rotulo de sessao). E so descritiva: NAO muda o veredito abaixo.
    # "uptime" = t_isr do 1o registro do arquivo; se cresce de um arquivo para o outro, nao houve reset entre eles.
    L += ["", "Por janela de captura (arquivo) - descritivo, nao substitui o rotulo de sessao:", "",
          "| Arquivo | uptime no inicio (s) | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |",
          "|---|---|---|---|---|---|---|---|---|"]
    janelas = sorted({r["arquivo"] for r in t4}, key=lambda f: min(x["t_isr_us"] for x in t4 if x["arquivo"] == f))
    for f in janelas:
        rows = [r for r in t4 if r["arquivo"] == f]
        L.append(f"| {f} | {min(r['t_isr_us'] for r in rows) / 1e6:.1f} " + fmt(describe([r["lat_saida_us"] for r in rows])))
    L += ["", f"Protocolo (>= {MIN_LAT_VALIDAS} medicoes validas em >= {MIN_SESSOES} sessoes para T4): "
          f"{'ATENDIDO' if ok_lat else 'NAO atendido'} ({len(t4)} validas, {sess} sessoes).",
          *([f"Situacao: PARCIALMENTE atendido - volume atingido ({len(t4)} validas em {len(janelas)} janelas de captura), "
             f"mas as janelas nao foram separadas como sessoes ({sess} rotulo de sessao)."]
            if not ok_lat and len(t4) >= MIN_LAT_VALIDAS and len(janelas) >= MIN_SESSOES else []),
          "Percentis de cauda (p99) nao sao reportados: amostra insuficiente para evidencia robusta.",
          "Latencia fim a fim (fisica) so pode ser afirmada com analisador logico.", ""]
    if not ok_lat and lat:
        # Evidencia exploratoria: TODOS os registros LAT (aquecimento e incidentais incluidos).
        # Mesmo trecho instrumentado (ISR -> FSM -> GPIO), mas sem controle de estimulo: NAO substitui o protocolo.
        L += ["### Q2 - Evidencia exploratoria (NAO atende ao protocolo)", "",
              "Todos os registros LAT do ESP32, incluindo aquecimento e acionamentos incidentais de botao",
              "durante os ensaios de falha. Mesmo trecho cronometrado (esp_timer_get_time); mistura condicoes",
              "diferentes (com e sem estimulo controlado): serve apenas como ordem de grandeza.", "",
              "| Grupo | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |", "|---|---|---|---|---|---|---|---|"]
        grp = [("todos (ISR -> GPIO)", lat),
               ("dentro do experimento lat (aquecimento)", [r for r in lat if r["controlado"]]),
               ("incidentais (fora do experimento lat)", lat_incid),
               ("somente T4 (inclui aquecimento)", [r for r in lat if r["transicao"] == "T4"])]
        for nome, rows in grp:
            if rows:
                L.append(f"| {nome} " + fmt(describe([r["lat_saida_us"] for r in rows])))
        L.append("")

    # ---------------- Q3
    L += ["## Q3 - Convergencia ao estado seguro (FALHA, amarelo piscante)", "",
          "Criterio: verdes apagados e FALHA alcancado em ate 1 s apos a injecao, sem retorno ao verde na janela.", "",
          "| Tipo de falha | injecoes validas | sucessos | taxa de sucesso | conv. media (ms) | mediana | DP | IIQ | min | max |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    ok_inj = True
    for t in sorted({r["tipo"] for r in faults_valid}):
        rows = [r for r in faults_valid if r["tipo"] == t]
        suc = sum(r["sucesso"] for r in rows)
        d = describe([r["conv_ms"] for r in rows if r["conv_ms"] >= 0])
        ok_inj &= len(rows) >= MIN_INJECOES or t == 4
        if d:
            L.append(f"| {TIPOS.get(t, t)} | {len(rows)} | {suc} | {100 * suc / len(rows):.1f}% | {d['media']:.2f} | "
                     f"{d['mediana']:.2f} | {d['dp']:.2f} | {d['iqr']:.2f} | {d['min']:.2f} | {d['max']:.2f} |")
        else:
            L.append(f"| {TIPOS.get(t, t)} | {len(rows)} | {suc} | {100 * suc / max(1, len(rows)):.1f}% | - | - | - | - | - | - |")
    L += ["", f"Protocolo (>= {MIN_INJECOES} injecoes validas por tipo 1-3): {'ATENDIDO' if ok_inj and faults_valid else 'NAO atendido'}.", ""]

    L += ["## Recuperacao via reset (avaliada separadamente)", "",
          "| Tipo de falha anterior | ensaios | sucesso | ms ate RECOVERY (mediana) | ms ate VERDE (mediana) |", "|---|---|---|---|---|"]
    for t in sorted({r["tipo"] for r in recs}):
        rows = [r for r in recs if r["tipo"] == t]
        okr = [r for r in rows if r["sucesso"]]
        L.append(f"| {TIPOS.get(t, t)} | {len(rows)} | {len(okr)} | "
                 f"{st.median([r['ms_ate_recovery'] for r in okr]) if okr else '-'} | "
                 f"{st.median([r['ms_ate_verde'] for r in okr]) if okr else '-'} |")

    L += ["", "## Descartes", "", f"Registros #DESCARTE emitidos pelo firmware: {len(desc)}"]
    L += [f"- {d['arquivo']}:{d['linha']} {d['registro']}" for d in desc]
    manual = [r for r in lat + faults if r.get("descartado")]
    L += [f"- manual: {r['arquivo']} linha {r['linha']} -> {r['descartado']}" for r in manual]
    L += ["", plots(lat_valid, faults_valid)]
    txt = "\n".join(L) + "\n"
    with open(os.path.join(OUT, "resumo.md"), "w", encoding="utf-8") as f:
        f.write(txt)
    print(txt)
    print(f"Arquivos gerados em {os.path.relpath(OUT, ROOT)}/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
