#!/usr/bin/env python3
"""
capture_serial.py - Captura o log serial do ESP32 e (opcionalmente) dispara experimentos.

Grava TODAS as linhas recebidas (registro bruto) em um arquivo de data/raw/.
Encerra quando todos os experimentos enviados terminarem (#FIM), ao atingir
--tempo-max, ou com Ctrl+C.

Exemplos:
    python tools/capture_serial.py --porta COM3 --saida data/raw/s1_lat.log --cmd "sessao 1" --cmd "lat 10"
    python tools/capture_serial.py --porta COM3 --saida data/raw/s1_falha1.log --cmd "sessao 1" --cmd "falha 1 50"
    python tools/capture_serial.py --porta /dev/ttyUSB0 --saida data/raw/livre.log      # so escuta

Requer: python -m pip install pyserial
Feche o 'idf.py monitor' antes (a porta so pode ter um dono).
"""
import argparse
import os
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial nao instalado: python -m pip install pyserial")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--porta", required=True, help="COM3, /dev/ttyUSB0, ...")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--saida", required=True, help="arquivo de log bruto (ex.: data/raw/s1_lat.log)")
    ap.add_argument("--cmd", action="append", default=[], help="comando a enviar (pode repetir)")
    ap.add_argument("--tempo-max", type=float, default=4 * 3600, help="tempo maximo em segundos")
    a = ap.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(a.saida)), exist_ok=True)
    experimentos = sum(1 for c in a.cmd if c.split()[0] in ("lat", "falha"))
    with serial.Serial(a.porta, a.baud, timeout=0.2) as ser, open(a.saida, "a", encoding="utf-8") as out:
        out.write(f"# captura iniciada {time.strftime('%Y-%m-%d %H:%M:%S')} porta={a.porta} cmds={a.cmd}\n")
        time.sleep(0.5)
        ser.reset_input_buffer()
        for c in a.cmd:
            ser.write((c + "\n").encode())
            out.write(f"# >> {c}\n")
            time.sleep(0.3)
        fins, t0, buf = 0, time.time(), b""
        try:
            while time.time() - t0 < a.tempo_max:
                buf += ser.read(512)
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    line = raw.decode(errors="replace").rstrip("\r")
                    if not line.strip():
                        continue
                    out.write(line + "\n")
                    out.flush()
                    print(line)
                    if line.startswith("#FIM"):
                        fins += 1
                if experimentos and fins >= experimentos:
                    print(f"\nTodos os {experimentos} experimento(s) concluidos.")
                    break
        except KeyboardInterrupt:
            print("\nInterrompido pelo usuario.")
    print(f"Log bruto salvo em {a.saida}")


if __name__ == "__main__":
    main()
