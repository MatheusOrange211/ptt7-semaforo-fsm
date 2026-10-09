#!/usr/bin/env python3
"""
run_esbmc.py - Camada 2 da verificacao: executa os harnesses ESBMC sobre core/fsm_core.c.

Uso:
    python verification/esbmc/run_esbmc.py                # todas as consultas
    python verification/esbmc/run_esbmc.py --rapido        # pula o segmento longo de P1 (seg 3, ~1 min)
    python verification/esbmc/run_esbmc.py --mutante       # compila o nucleo com -DFSM_INJECT_BUG (deve REFUTAR P6)
    python verification/esbmc/run_esbmc.py --esbmc /caminho/esbmc

Saidas:
    verification/results/esbmc_results.json   (ou esbmc_results_mutante.json)
    verification/results/esbmc_logs/*.txt     (saida completa de cada consulta, inclusive contraexemplos)

Requer o binario `esbmc` no PATH, na variavel de ambiente ESBMC ou via --esbmc.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
CORE = os.path.join(ROOT, "core")
RESULTS = os.path.join(ROOT, "verification", "results")
LOGS = os.path.join(RESULTS, "esbmc_logs")

# (id, propriedade, descricao, harness, defines, opcoes, tipo)
#   tipo: "seguranca"  -> SUCCESSFUL = PROVADO ; FAILED = REFUTADO
#         "limite"     -> SUCCESSFUL = VALIDO_NO_LIMITE
#         "kind"       -> k-inducao: SUCCESSFUL = PROVADO
#         "alcance"    -> FAILED (contraexemplo = caminho) = ALCANCADO ; SUCCESSFUL = INCONCLUSIVO
OVF = ["--overflow-check", "--unsigned-overflow-check"]
JOBS = [
    ("step-P2", "P2", "transicao definida p/ toda entrada; dominio preservado (1 passo, contexto arbitrario)",
     "harness_step.c", ["-DPROP_P2"], OVF, "seguranca"),
    ("step-P3", "P3", "exclusao mutua dos verdes; verde de pedestre so em VP (1 passo)",
     "harness_step.c", ["-DPROP_P3"], OVF, "seguranca"),
    ("step-P3b", "P3", "verde veicular so termina apos T_VERDE_MIN (1 passo)",
     "harness_step.c", ["-DPROP_P3B"], OVF, "seguranca"),
    ("step-P4", "P4", "entrada com falha leva a FALHA (1 passo)",
     "harness_step.c", ["-DPROP_P4"], OVF, "seguranca"),
    ("step-P6", "P6", "de FALHA so se sai para RECOVERY (1 passo)",
     "harness_step.c", ["-DPROP_P6"], OVF, "seguranca"),
    ("step-mem", "P2", "seguranca de memoria e aritmetica do nucleo (todas as assercoes + overflow)",
     "harness_step.c", [], OVF + ["--memory-leak-check"], "seguranca"),
    ("bmc-k10", "P5", "BMC desde INIT, k=10: P3, falha->FALHA no passo seguinte e mantida, P6",
     "harness_bmc.c", ["-DK=10"], ["--unwind", "11"] + OVF, "limite"),
    ("kind-P5", "P5", "k-inducao desde INIT (laco infinito): P3, P4/P5, P6 sem limite de passos",
     "harness_bmc.c", ["-DUNBOUNDED"], ["--k-induction", "--max-k-step", "10"], "kind"),
]
SEGMENTS = [
    (1, "INIT -> VERDE_VEICULO", 12), (2, "VERDE_VEICULO -> PEDIDO_PEDESTRE", 3),
    (3, "PEDIDO_PEDESTRE -> ATENCAO_VEICULO", 42), (4, "ATENCAO_VEICULO -> VERMELHO_VEICULO_VERDE_PEDESTRE", 17),
    (5, "VERMELHO_VEICULO_VERDE_PEDESTRE -> ATENCAO_PEDESTRE", 32), (6, "ATENCAO_PEDESTRE -> VERDE_VEICULO", 17),
    (7, "INIT -> FALHA", 3), (8, "FALHA -> RECOVERY", 3),
]
for seg, desc, unwind in SEGMENTS:
    JOBS.append((f"reach-seg{seg}", "P1", f"alcancabilidade do trecho {desc}",
                 "harness_reach.c", [f"-DSEG={seg}"], ["--unwind", str(unwind), "--no-unwinding-assertions"], "alcance"))


def find_esbmc(cli):
    for cand in [cli, os.environ.get("ESBMC"), shutil.which("esbmc")]:
        if cand and os.path.isfile(cand):
            return cand
    return None


def esbmc_version(exe):
    try:
        out = subprocess.run([exe, "--version"], capture_output=True, text=True, timeout=30).stdout
        return out.strip().splitlines()[0] if out.strip() else "desconhecida"
    except Exception:
        return "desconhecida"


def run_job(exe, job, mutante, timeout):
    jid, prop, desc, harness, defs, opts, kind = job
    cmd = [exe, os.path.join(HERE, harness), os.path.join(CORE, "fsm_core.c"),
           "-I", CORE, "-I", HERE, "--z3"] + defs + opts
    if mutante:
        cmd.append("-DFSM_INJECT_BUG")
    t0 = time.time()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=HERE)
        out = p.stdout + p.stderr
        timed_out = False
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
        timed_out = True
    secs = time.time() - t0
    os.makedirs(LOGS, exist_ok=True)
    suffix = "_mutante" if mutante else ""
    with open(os.path.join(LOGS, f"{jid}{suffix}.txt"), "w", encoding="utf-8") as f:
        f.write("$ " + " ".join(cmd) + "\n\n" + out)

    ok = "VERIFICATION SUCCESSFUL" in out
    failed = "VERIFICATION FAILED" in out
    detail = ""
    if timed_out or (not ok and not failed):
        status, detail = "INCONCLUSIVO", ("timeout" if timed_out else "sem veredito (ver log)")
    elif kind == "alcance":
        status = "ALCANCADO" if failed else "INCONCLUSIVO"
        detail = "contraexemplo = caminho testemunha" if failed else "nao alcancado no limite"
    elif failed:
        status = "REFUTADO"
        m = re.search(r"Violated property:\s*\n\s*(.*)\n\s*(.*)\n", out)
        detail = (m.group(1).strip() + " | " + m.group(2).strip()) if m else "ver log"
    elif kind == "limite":
        status, detail = "VALIDO_NO_LIMITE", "k=10 passos"
    elif kind == "kind":
        m = re.search(r"Solution found by the (\w+) step \(k = (\d+)\)", out)
        status = "PROVADO"
        detail = f"k-inducao ({m.group(1)} step, k={m.group(2)})" if m else "k-inducao"
    else:
        status, detail = "PROVADO", "todos os contextos validos (1 passo)"
    return {"id": jid, "propriedade": prop, "descricao": desc, "status": status, "detalhe": detail,
            "tempo_s": round(secs, 2), "comando": " ".join(os.path.relpath(c, ROOT) if os.path.isabs(c) else c for c in cmd)}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--esbmc", help="caminho do executavel esbmc")
    ap.add_argument("--mutante", action="store_true", help="nucleo com -DFSM_INJECT_BUG (teste negativo)")
    ap.add_argument("--rapido", action="store_true", help="pula o segmento 3 de alcancabilidade (~1 min)")
    ap.add_argument("--timeout", type=int, default=600, help="timeout por consulta (s)")
    args = ap.parse_args()

    exe = find_esbmc(args.esbmc)
    if not exe:
        print("ERRO: esbmc nao encontrado. Instale (ver MANUAL_EXECUCAO.md, secao 2) e coloque no PATH,\n"
              "      ou informe --esbmc /caminho/esbmc ou a variavel de ambiente ESBMC.")
        return 2
    ver = esbmc_version(exe)
    print(f"{ver} | backend Z3" + (" | MODO MUTANTE (-DFSM_INJECT_BUG)" if args.mutante else ""))
    print("-" * 100)
    results = []
    for job in JOBS:
        if args.rapido and job[0] == "reach-seg3":
            continue
        r = run_job(exe, job, args.mutante, args.timeout)
        mark = {"PROVADO": "OK ", "ALCANCADO": "OK ", "VALIDO_NO_LIMITE": "OK*", "REFUTADO": "XX ",
                "INCONCLUSIVO": "?? "}[r["status"]]
        print(f"[{mark}] {r['id']:<12} {r['propriedade']:<3} {r['status']:<17} {r['tempo_s']:>7.2f}s  {r['descricao']}"
              + (f"  ({r['detalhe']})" if r["detalhe"] else ""), flush=True)
        results.append(r)

    os.makedirs(RESULTS, exist_ok=True)
    name = "esbmc_results_mutante.json" if args.mutante else "esbmc_results.json"
    with open(os.path.join(RESULTS, name), "w", encoding="utf-8") as f:
        json.dump({"ferramenta": ver, "camada": "codigo", "mutante": args.mutante,
                   "data": time.strftime("%Y-%m-%d %H:%M:%S"), "resultados": results}, f, ensure_ascii=False, indent=2)
    bad = [r for r in results if r["status"] in ("REFUTADO", "INCONCLUSIVO")]
    print("-" * 100)
    print(f"Resumo ESBMC: {len(results)} consultas, {len(bad)} refutadas/inconclusivas. "
          f"Resultados: verification/results/{name}; logs: verification/results/esbmc_logs/")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
