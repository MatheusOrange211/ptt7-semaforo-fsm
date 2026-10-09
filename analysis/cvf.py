#!/usr/bin/env python3
"""
cvf.py - Consolida CVF-Modelo (Z3) e CVF-Codigo (ESBMC) por propriedade P1..P6.

CVF = numero e proporcao das seis propriedades por categoria de resultado:
    comprovado | valido_no_limite | refutado | inconclusivo
(CVF NAO e cobertura de codigo nem de todos os comportamentos.)

Regra de agregacao por propriedade (pior caso das consultas que a compoem):
    algum REFUTADO -> refutado; senao algum INCONCLUSIVO -> inconclusivo;
    senao algum VALIDO_NO_LIMITE -> valido_no_limite; senao -> comprovado.
Observacao: em uma camada, uma consulta VALIDO_NO_LIMITE complementada por prova
ilimitada da MESMA propriedade (ex.: k-inducao) conta como comprovado.

Uso: python analysis/cvf.py [--sufixo _mutante]
Saida: verification/results/cvf.md e cvf.json
"""
import argparse
import json
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
RES = os.path.join(ROOT, "verification", "results")
PROPS = ["P1", "P2", "P3", "P4", "P5", "P6"]
NOMES = {
    "P1": "alcancabilidade dos estados",
    "P2": "ausencia de deadlock / transicao definida",
    "P3": "exclusao mutua dos verdes (+ verde minimo)",
    "P4": "violacao das entradas -> estado seguro",
    "P5": "convergencia do fallback",
    "P6": "FALHA nao retorna direto a VP",
}
CAT = {"PROVADO": "comprovado", "ALCANCADO": "comprovado", "VALIDO_NO_LIMITE": "valido_no_limite",
       "REFUTADO": "refutado", "INCONCLUSIVO": "inconclusivo"}
ORDER = ["refutado", "inconclusivo", "valido_no_limite", "comprovado"]


def prop_of(item):
    pid = item.get("propriedade", "")
    m = re.match(r"(P\d)", pid)
    return m.group(1) if m else None


def aggregate(items):
    by = {p: [] for p in PROPS}
    for it in items:
        p = prop_of(it)
        if p in by:
            by[p].append(CAT.get(it["status"], "inconclusivo"))
    out = {}
    for p, cats in by.items():
        if not cats:
            out[p] = "nao_avaliado"
            continue
        worst = min(cats, key=ORDER.index)
        if worst == "valido_no_limite" and "comprovado" in cats and p == "P5":
            # limite (BMC) complementado por prova ilimitada da mesma propriedade
            unbounded = [it for it in items if prop_of(it) == p and it["status"] == "PROVADO"]
            if unbounded:
                worst = "comprovado"
        out[p] = worst
    return out


def load(name):
    path = os.path.join(RES, name)
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sufixo", default="", help="ex.: _mutante")
    a = ap.parse_args()
    z3r = load(f"z3_results{a.sufixo}.json")
    esr = load(f"esbmc_results{a.sufixo}.json")
    camadas = {}
    if z3r:
        camadas["Modelo (Z3)"] = aggregate(z3r["resultados"])
    if esr:
        camadas["Codigo (ESBMC)"] = aggregate(esr["resultados"])
    if not camadas:
        print("Nenhum resultado encontrado em verification/results/. Rode 'python tools/ptt7.py z3' e/ou 'esbmc'.")
        return 1

    lines = ["# CVF - resultados da verificacao em duas camadas", ""]
    if z3r:
        lines.append(f"- Modelo: {z3r.get('ferramenta')} - {z3r.get('data')} - horizonte BMC k={z3r.get('horizonte_bmc')}")
    if esr:
        lines.append(f"- Codigo: {esr.get('ferramenta')} - {esr.get('data')}")
    lines += ["", "| Propriedade | Descricao | " + " | ".join(camadas) + " | Concordancia |",
              "|---|---|" + "---|" * len(camadas) + "---|"]
    for p in PROPS:
        vals = [camadas[c][p] for c in camadas]
        conc = "sim" if len(set(vals)) == 1 else "NAO"
        lines.append(f"| {p} | {NOMES[p]} | " + " | ".join(vals) + f" | {conc} |")
    lines += ["", "| Camada | comprovado | valido_no_limite | refutado | inconclusivo |", "|---|---|---|---|---|"]
    resumo = {}
    for c, agg in camadas.items():
        cnt = {k: sum(1 for v in agg.values() if v == k) for k in ["comprovado", "valido_no_limite", "refutado", "inconclusivo"]}
        resumo[c] = cnt
        lines.append(f"| {c} | " + " | ".join(f"{cnt[k]} ({100 * cnt[k] / 6:.0f}%)" for k in cnt) + " |")
    lines += ["", "CVF registra o numero e a proporcao das seis propriedades por categoria de resultado; "
              "nao representa cobertura de codigo nem de todos os comportamentos."]
    txt = "\n".join(lines) + "\n"
    print(txt)
    with open(os.path.join(RES, f"cvf{a.sufixo}.md"), "w", encoding="utf-8") as f:
        f.write(txt)
    with open(os.path.join(RES, f"cvf{a.sufixo}.json"), "w", encoding="utf-8") as f:
        json.dump({"por_propriedade": camadas, "contagem": resumo}, f, ensure_ascii=False, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
