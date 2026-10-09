#!/usr/bin/env python3
"""
ptt7.py - Orquestrador do projeto PTT7 (host: Windows, Linux, WSL ou macOS).

Comandos:
    python tools/ptt7.py check            verifica ferramentas instaladas e versoes
    python tools/ptt7.py build            compila binarios de host em build/
    python tools/ptt7.py test             testes unitarios + conformidade modelo x codigo + testes negativos (mutante)
    python tools/ptt7.py z3 [--k 10] [--mutante]     camada 1 (modelo, Z3 API C++)
    python tools/ptt7.py esbmc [--rapido] [--mutante] camada 2 (codigo C, ESBMC)
    python tools/ptt7.py verify           z3 + esbmc + tabela CVF (analysis/cvf.py)
    python tools/ptt7.py sim [args]       simulador de host (Plano B) - ver MANUAL_EXECUCAO.md
    python tools/ptt7.py all              build + test + verify
    python tools/ptt7.py versions         grava verification/results/versoes.txt (reprodutibilidade)

Variaveis de ambiente opcionais:
    CC / CXX     compiladores C / C++ (padrao: gcc / g++)
    Z3_DIR       pasta com include/z3++.h e lib/ (padrao: a do pacote pip z3-solver)
    ESBMC        caminho do executavel esbmc
"""
import glob
import os
import platform
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
BUILD = os.path.join(ROOT, "build")
IS_WIN = os.name == "nt"
EXE = ".exe" if IS_WIN else ""
CC = os.environ.get("CC", "gcc")
CXX = os.environ.get("CXX", "g++")
CFLAGS = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror"]
CXXFLAGS = ["-std=c++17", "-O2", "-Wall"]


def sh(cmd, check=True, cwd=ROOT, **kw):
    print("$ " + " ".join(cmd), flush=True)
    r = subprocess.run(cmd, cwd=cwd, **kw)
    if check and r.returncode != 0:
        raise SystemExit(f"comando falhou (codigo {r.returncode})")
    return r.returncode


def p(*parts):
    return os.path.join(ROOT, *parts)


# ------------------------------------------------------------------- Z3
def z3_paths():
    """Retorna (include_dir, lib_dir, arquivo_lib) do Z3 para a API C++."""
    base = os.environ.get("Z3_DIR")
    if not base:
        try:
            import z3  # noqa: F401  (pacote pip z3-solver traz include/ e lib/)
            base = os.path.dirname(z3.__file__)
        except ImportError:
            return None
    inc = os.path.join(base, "include")
    libdir = os.path.join(base, "lib")
    if not os.path.isdir(libdir):
        libdir = os.path.join(base, "bin")
    if not os.path.isfile(os.path.join(inc, "z3++.h")):
        return None
    cands = []
    for pat in ("libz3.dll", "libz3.so", "libz3.dylib", "libz3.lib", "libz3.so.*", "z3.lib"):
        cands += glob.glob(os.path.join(libdir, pat))
        cands += glob.glob(os.path.join(base, "bin", pat))
    return (inc, libdir, cands[0]) if cands else None


def z3_link_args():
    zp = z3_paths()
    if not zp:
        raise SystemExit("ERRO: Z3 (API C++) nao encontrado. Rode:  python -m pip install z3-solver\n"
                         "      ou defina Z3_DIR apontando para uma distribuicao do Z3 (com include/ e bin/ ou lib/).")
    inc, libdir, libfile = zp
    args = ["-I" + inc]
    if IS_WIN:
        args.append(libfile)                      # MinGW-w64 liga direto na DLL
    else:
        args += ["-L" + libdir, "-lz3", "-Wl,-rpath," + libdir]
        if not glob.glob(os.path.join(libdir, "libz3.so")) and not glob.glob(os.path.join(libdir, "libz3.dylib")):
            args = ["-I" + inc, libfile, "-Wl,-rpath," + libdir]
    return args, zp


# ---------------------------------------------------------------- build
def build(with_z3=True):
    os.makedirs(BUILD, exist_ok=True)
    core = [p("core", "fsm_core.c")]
    inc = ["-I" + p("core")]
    sh([CC] + CFLAGS + inc + core + [p("tests", "test_fsm_core.c"), "-o", p("build", "test_fsm_core" + EXE)])
    sh([CC] + CFLAGS + ["-DFSM_INJECT_BUG"] + inc + core + [p("tests", "test_fsm_core.c"),
                                                           "-o", p("build", "test_fsm_core_mutante" + EXE)])
    sh([CC] + CFLAGS + inc + core + [p("sim", "sim_main.c"), "-o", p("build", "ptt7_sim" + EXE)])
    for name, extra in (("fsm_core.o", []), ("fsm_core_mutante.o", ["-DFSM_INJECT_BUG"])):
        sh([CC] + CFLAGS + extra + inc + ["-c", core[0], "-o", p("build", name)])
    sh([CXX] + CXXFLAGS + [p("verification", "z3", "conformance.cpp"), p("build", "fsm_core.o"),
                           "-o", p("build", "conformance" + EXE)])
    sh([CXX] + CXXFLAGS + [p("verification", "z3", "conformance.cpp"), p("build", "fsm_core_mutante.o"),
                           "-o", p("build", "conformance_mutante" + EXE)])
    if with_z3:
        zargs, zp = z3_link_args()
        sh([CXX] + CXXFLAGS + ["-Wno-return-type", p("verification", "z3", "verify_z3.cpp"),
                               "-o", p("build", "verify_z3" + EXE)] + zargs)
        if IS_WIN:   # a DLL precisa estar ao lado do executavel
            for dll in glob.glob(os.path.join(zp[1], "*.dll")) + glob.glob(os.path.join(os.path.dirname(zp[1]), "bin", "*.dll")):
                shutil.copy2(dll, BUILD)
    print("Build OK ->", BUILD)


def ensure_built(name):
    if not os.path.isfile(p("build", name + EXE)):
        build(with_z3=(name == "verify_z3"))


# ----------------------------------------------------------------- test
def test():
    ensure_built("test_fsm_core")
    ensure_built("conformance")
    os.makedirs(p("verification", "results"), exist_ok=True)
    ensure_built("ptt7_sim")
    print("\n=== 1/5 Testes unitarios do nucleo ===")
    sh([p("build", "test_fsm_core" + EXE)])
    print("\n=== 2/5 Cenarios de aceitacao no simulador (tempo virtual) ===")
    falhos = []
    for cen in sorted(glob.glob(p("sim", "cenarios", "*.txt"))):
        r = subprocess.run([p("build", "ptt7_sim" + EXE), "--cenario", cen], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or ["?"])[-1]
        print(f"  {os.path.basename(cen):<32} {last}")
        if r.returncode != 0:
            falhos.append(cen)
            print(r.stdout)
    if falhos:
        raise SystemExit(f"ERRO: cenarios falharam: {falhos}")
    print("\n=== 3/5 Conformidade modelo (C++) x codigo (C) com os mesmos vetores ===")
    sh([p("build", "conformance" + EXE)])
    print("\n=== 4/5 Teste negativo: testes unitarios contra o MUTANTE (devem FALHAR) ===")
    r1 = sh([p("build", "test_fsm_core_mutante" + EXE)], check=False, stdout=subprocess.DEVNULL)
    print("\n=== 5/5 Teste negativo: conformidade contra o MUTANTE (deve divergir) ===")
    r2 = sh([p("build", "conformance_mutante" + EXE), "--vectors", p("build", "vetores_mutante.csv")],
            check=False, stdout=subprocess.DEVNULL)
    if r1 == 0 or r2 == 0:
        raise SystemExit("ERRO: o mutante NAO foi detectado - os testes perderam poder de deteccao.")
    print("\nMutante detectado pelos dois testes (esperado).")
    print("TESTES: OK")


def z3_cmd(extra):
    ensure_built("verify_z3")
    os.makedirs(p("verification", "results"), exist_ok=True)
    return sh([p("build", "verify_z3" + EXE)] + extra, check=False)


def esbmc_cmd(extra):
    return sh([sys.executable, p("verification", "esbmc", "run_esbmc.py")] + extra, check=False)


def verify(extra):
    rc1 = z3_cmd([a for a in extra if a in ("--mutante",)] )
    rc2 = esbmc_cmd([a for a in extra if a in ("--mutante", "--rapido")])
    sh([sys.executable, p("analysis", "cvf.py")], check=False)
    return rc1 or rc2


def tool_version(cmd):
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        txt = (out.stdout or out.stderr).strip().splitlines()
        return txt[0] if txt else "?"
    except Exception:
        return None


def versions(write=True):
    lines = [f"Plataforma: {platform.platform()}", f"Python: {sys.version.split()[0]}"]
    lines.append(f"{CC}: {tool_version([CC, '--version']) or 'NAO ENCONTRADO'}")
    lines.append(f"{CXX}: {tool_version([CXX, '--version']) or 'NAO ENCONTRADO'}")
    try:
        import z3
        lines.append(f"z3-solver (pip): {z3.get_version_string()}")
    except ImportError:
        lines.append("z3-solver (pip): NAO ENCONTRADO")
    zp = z3_paths()
    lines.append(f"Z3 API C++: {'OK ' + zp[2] if zp else 'NAO ENCONTRADA'}")
    esb = os.environ.get("ESBMC") or shutil.which("esbmc")
    lines.append(f"ESBMC: {tool_version([esb, '--version']) if esb else 'NAO ENCONTRADO'}")
    idf = shutil.which("idf.py") or shutil.which("idf.py.exe")
    lines.append(f"ESP-IDF (idf.py): {'encontrado' if idf else 'nao encontrado neste terminal (normal fora do ESP-IDF Terminal)'}")
    try:
        import serial  # noqa: F401
        lines.append("pyserial: OK")
    except ImportError:
        lines.append("pyserial: NAO ENCONTRADO (necessario so para captura serial do ESP32)")
    try:
        commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True,
                                text=True).stdout.strip()
    except Exception:
        commit = ""
    lines.append(f"Commit do nucleo: {commit or 'repositorio git ainda nao inicializado'}")
    print("\n".join(lines))
    if write:
        os.makedirs(p("verification", "results"), exist_ok=True)
        with open(p("verification", "results", "versoes.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    return lines


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print(__doc__)
        return 0
    cmd, extra = sys.argv[1], sys.argv[2:]
    if cmd == "check":
        versions(write=False)
    elif cmd == "versions":
        versions(write=True)
    elif cmd == "build":
        build(with_z3="--sem-z3" not in extra)
    elif cmd == "test":
        test()
    elif cmd == "z3":
        return z3_cmd(extra)
    elif cmd == "esbmc":
        return esbmc_cmd(extra)
    elif cmd == "verify":
        return verify(extra)
    elif cmd == "sim":
        ensure_built("ptt7_sim")
        return sh([p("build", "ptt7_sim" + EXE)] + extra, check=False)
    elif cmd == "all":
        build()
        test()
        versions(write=True)
        return verify(extra)
    else:
        print(f"comando desconhecido: {cmd}\n{__doc__}")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
