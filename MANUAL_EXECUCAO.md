# Manual de execução e testes — PTT7

**Controle de semáforo com travessia de pedestres baseado em FSM no ESP32, com verificação em duas camadas**
Pedro Ribeiro e Matheus Correa · Orientador: Prof. Herbert Rocha · PPCA/UFRR · 2026.2

Este manual explica como instalar as ferramentas, executar cada parte do projeto e conferir se o
resultado está correto. Siga as seções em ordem. A **seção 3** é o roteiro rápido (cerca de 15 min,
sem placa). As seções 7 a 9 exigem a bancada com o ESP32.

---

## Sumário

1. [O que já está pronto](#1-o-que-já-está-pronto)
2. [Instalação das ferramentas](#2-instalação-das-ferramentas)
3. [Roteiro rápido: testar tudo sem placa](#3-roteiro-rápido-testar-tudo-sem-placa)
4. [Testes de host em detalhe](#4-testes-de-host-em-detalhe)
5. [Verificação formal (Z3 e ESBMC)](#5-verificação-formal-z3-e-esbmc)
6. [Simulador — Plano B](#6-simulador--plano-b)
7. [Firmware no ESP32](#7-firmware-no-esp32)
8. [Experimentos de bancada (Q2 e Q3)](#8-experimentos-de-bancada-q2-e-q3)
9. [Análise dos dados e relatório](#9-análise-dos-dados-e-relatório)
10. [Checklist de aceitação](#10-checklist-de-aceitação)
11. [Solução de problemas](#11-solução-de-problemas)

---

## 1. O que já está pronto

| Pasta / arquivo | Conteúdo | Objetivo |
|---|---|---|
| `spec/transition_table.csv`, `spec/fsm_spec.md` | 8 estados, 16 transições (T1–T16), saídas, temporizações, propriedades P1–P6, diagrama | OE1 |
| `core/fsm_core.c/.h`, `core/fsm_config.h` | Núcleo funcional em C puro, sem hardware, tempo como entrada explícita. **É o mesmo arquivo** compilado no firmware e analisado pelo ESBMC | OE2 |
| `firmware/` | Projeto ESP-IDF v5.1: GPIO, ISR em IRAM, tarefa FreeRTOS, medição de latência, injeção de falhas, console de experimentos | OE2, OE4 |
| `verification/z3/` | Modelo abstrato em **Z3 via API C++** (`verify_z3.cpp`) e teste de conformidade modelo × código (`conformance.cpp`) | OE3 |
| `verification/esbmc/` | Harnesses ESBMC sobre `fsm_core.c` e executor `run_esbmc.py` | OE3 |
| `verification/results/` | Saídas das verificações (JSON, logs, CVF, vetores) | OE3, OE5 |
| `tests/test_fsm_core.c` | 336 verificações unitárias do núcleo | OE2 |
| `sim/` | Simulador de host (Plano B) e 7 cenários de aceitação | Plano B |
| `tools/ptt7.py` | Orquestrador: `check`, `build`, `test`, `z3`, `esbmc`, `verify`, `sim`, `all` | — |
| `tools/capture_serial.py` | Captura do log serial e disparo dos experimentos | OE4 |
| `analysis/analyze.py`, `analysis/cvf.py` | Estatísticas (média, mediana, DP, IIQ, mín., máx.) e tabela CVF | OE4, OE5 |
| `docs/bancada.md` | Esquema de ligação | — |
| `docs/registro_verificacao.md` | Premissas, limites, contraexemplos e correções | OE3 |

**Resultados já obtidos no desenvolvimento** (06/10/2026, Z3 5.1.0 e ESBMC 8.5.0):

- Testes unitários: 336/336. Cenários de aceitação: 7/7. Conformidade: 100 000 passos, 16/16 transições, 0 divergências.
- CVF-Modelo e CVF-Código: P1–P6 **comprovadas** nas duas camadas (ver `verification/results/cvf.md`).
- O ESBMC encontrou e ajudou a corrigir um *overflow* aritmético em `sat_add` (achado ESBMC-01, em `docs/registro_verificacao.md`).
- O firmware compila no ESP-IDF v5.1.4 sem avisos. `fsm_step` e `fsm_outputs` ficam em IRAM.
- **Ainda não executado:** nada em placa física. Latências e taxas de fallback reais saem da seção 8.

---

## 2. Instalação das ferramentas

O projeto está em `C:\project\ptt7`. Há dois ambientes:

| Ambiente | Para quê | Recomendação |
|---|---|---|
| **WSL2 (Ubuntu)** | testes de host, Z3 (API C++), ESBMC, análise | **recomendado**, porque o ESBMC é distribuído para Linux |
| **Windows (ESP-IDF)** | compilar, gravar e monitorar o ESP32; capturar a serial | obrigatório para a bancada |

No WSL, a pasta do projeto aparece como `/mnt/c/project/ptt7`.

### 2.1 WSL2 + Ubuntu (uma vez)

No PowerShell **como administrador**:

```powershell
wsl --install -d Ubuntu-22.04
```

Reinicie o computador se ele pedir, abra o "Ubuntu" no menu Iniciar e crie usuário e senha. Depois, no terminal Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential python3 python3-pip python3-venv unzip git
cd /mnt/c/project/ptt7
python3 -m venv ~/ptt7-venv && source ~/ptt7-venv/bin/activate
python -m pip install -r requirements.txt        # z3-solver (inclui z3++.h), pyserial, matplotlib
```

> Em todo terminal novo, rode `source ~/ptt7-venv/bin/activate` antes de usar o projeto.

### 2.2 ESBMC (no WSL)

```bash
cd ~
wget https://github.com/esbmc/esbmc/releases/latest/download/esbmc-linux.zip
unzip -q esbmc-linux.zip -d esbmc
chmod +x ~/esbmc/release/bin/esbmc 2>/dev/null || chmod +x $(find ~/esbmc -name esbmc -type f)
echo "export ESBMC=$(find ~/esbmc -name esbmc -type f | head -1)" >> ~/.bashrc
source ~/.bashrc
$ESBMC --version            # esperado: ESBMC version 8.x ...
```

### 2.3 ESP-IDF v5.1 (no Windows)

1. Baixe o **ESP-IDF Tools Installer** (offline, versão **5.1.x**; usamos a 5.1.4) em
   <https://dl.espressif.com/dl/esp-idf/> e instale com as opções padrão (alvo ESP32).
2. Abra o atalho **"ESP-IDF 5.1 PowerShell"** (ou CMD) criado pelo instalador.
3. Instale o driver USB-serial da placa (CP210x ou CH340) se a porta COM não aparecer no Gerenciador de Dispositivos.
4. No mesmo terminal: `python -m pip install pyserial matplotlib`.

### 2.4 Conferir a instalação

No WSL:

```bash
cd /mnt/c/project/ptt7
python3 tools/ptt7.py check
```

Saída esperada (versões podem variar):

```
gcc: gcc (Ubuntu 11.x) ...
g++: g++ (Ubuntu 11.x) ...
z3-solver (pip): 4.x / 5.x
Z3 API C++: OK .../z3/lib/libz3.so
ESBMC: ESBMC version 8.x 64-bit x86_64 linux
```

> **Alternativa sem WSL (Windows nativo):** instale o MSYS2, rode `pacman -S mingw-w64-ucrt-x86_64-gcc`,
> ponha `C:\msys64\ucrt64\bin` no PATH e use `python -m pip install z3-solver`. `build`, `test`, `z3`
> e `sim` funcionam; o ESBMC continua precisando do WSL.

---

## 3. Roteiro rápido: testar tudo sem placa

No WSL, dentro de `/mnt/c/project/ptt7`:

```bash
python3 tools/ptt7.py build      # compila tudo em build/                         (~30 s)
python3 tools/ptt7.py test       # unitários + cenários + conformidade + mutante  (~10 s)
python3 tools/ptt7.py verify     # Z3 + ESBMC + tabela CVF                        (~5-8 min)
python3 tools/ptt7.py versions   # registra versões em verification/results/versoes.txt
```

Ou tudo de uma vez: `python3 tools/ptt7.py all`.

**Critério de sucesso:**

- `test` termina com `TESTES: OK`.
- `verify` termina com `Resumo Z3: 23 consultas, 0 refutadas/inconclusivas`,
  `Resumo ESBMC: 16 consultas, 0 refutadas/inconclusivas` e uma tabela CVF com
  todas as linhas `comprovado`.

---

## 4. Testes de host em detalhe

`python3 tools/ptt7.py test` executa cinco etapas:

| Etapa | O que verifica | Resultado esperado |
|---|---|---|
| 1. Unitários (`tests/test_fsm_core.c`) | as 16 transições, saídas de cada estado, verde mínimo, *latch* de demanda, falha em todos os estados e tipos, limite `dt = DT_MAX`, estado corrompido → FALHA, saturação | `Resultado: 336 verificacoes, 0 falhas` / `STATUS: OK` |
| 2. Cenários (`sim/cenarios/*.txt`) | 7 roteiros temporais de ponta a ponta (tabela abaixo) | cada linha termina com `-> OK` |
| 3. Conformidade (`conformance.cpp`) | os mesmos vetores aleatórios (semente 2026) aplicados ao `fsm_core.c` e ao modelo C++ do Z3; compara estado, transição, temporizadores, *latch* e saídas a cada passo | `transicoes cobertas: 16/16 \| divergencias: 0` / `STATUS: OK` |
| 4. Mutante × unitários | o núcleo compilado com `-DFSM_INJECT_BUG` (FALHA → VP) **precisa** falhar | (silencioso) |
| 5. Mutante × conformidade | o mutante **precisa** divergir do modelo | `Mutante detectado pelos dois testes (esperado).` |

Cenários de aceitação (tempo virtual, passo de 10 ms):

| Cenário | Verifica |
|---|---|
| `01_ciclo_normal` | INIT → VV (2 s); pedido com verde mínimo cumprido → T4; AV 3 s → VP 6 s → AP 3 s → VV |
| `02_verde_minimo` | pedido aos 1 s de verde → PEDIDO_PEDESTRE até completar 8 s (P3b) |
| `03/04_falha_sinal1/2` | perda de sinal → FALHA com verdes apagados; reset ignorado enquanto a falha persiste; restaurar + reset → RECOVERY → VV (P4, P5, P6) |
| `05_falha_simultanea_vp` | perda dos dois sinais **durante** VP → verde de pedestre apagado imediatamente |
| `06_falha_temporizacao` | passo atrasado 300 ms (> DT_MAX) → FALHA |
| `07_limite_temporizacao` | atraso exatamente 200 ms **não** é falha |

Para rodar um cenário isolado e ver a linha do tempo:

```bash
./build/ptt7_sim --cenario sim/cenarios/05_falha_simultanea_vp.txt
```

Os relatórios de vetores ficam em `verification/results/conformance_vectors.csv` (rastreabilidade: "mesmos vetores de entrada").

---

## 5. Verificação formal (Z3 e ESBMC)

### 5.1 Camada 1: modelo em Z3 (API C++)

```bash
python3 tools/ptt7.py z3                 # padrão: horizonte BMC k=10 (protocolo)
python3 tools/ptt7.py z3 --k 20          # revisão do horizonte de P5
python3 tools/ptt7.py z3 --mutante       # teste negativo: deve REFUTAR P6
```

O modelo (`verification/z3/fsm_model.hpp`) foi escrito a partir da **tabela de transições**, sem
reaproveitar o C, e lê as temporizações direto de `core/fsm_config.h`. Consultas:

| Consulta | Tipo | Resultado esperado |
|---|---|---|
| P2a | guardas mutuamente exclusivas (determinismo da tabela) | PROVADO |
| P2b | próximo estado definido e domínio preservado | PROVADO |
| P3, P3b | exclusão mútua dos verdes; verde mínimo | PROVADO |
| P4 | falha → FALHA no próximo passo | PROVADO |
| P6 | FALHA só sai para RECOVERY | PROVADO |
| P5i, P5 | passo indutivo + BMC k=10 desde INIT | PROVADO / VALIDO_NO_LIMITE |
| P1.0–P1.7 | cada estado alcançável a partir de INIT (k_ciclo = 133) | ALCANCADO (mostra um caminho testemunha, não o mínimo) |
| P2c.x | de qualquer contexto de cada estado volta a VERDE_VEICULO | PROVADO |

Saída: `verification/results/z3_results.json`, com contraexemplos quando houver.

### 5.2 Camada 2: código C com ESBMC

```bash
python3 tools/ptt7.py esbmc              # 16 consultas (~2-3 min)
python3 tools/ptt7.py esbmc --rapido     # pula o segmento longo de P1 (~1 min a menos)
python3 tools/ptt7.py esbmc --mutante    # teste negativo
```

| Consulta | Harness | Esperado |
|---|---|---|
| `step-P2`, `step-P3`, `step-P3b`, `step-P4`, `step-P6` | `harness_step.c`: contexto arbitrário válido + entrada arbitrária, com `--overflow-check --unsigned-overflow-check` | PROVADO |
| `step-mem` | todas as asserções + overflow + vazamento de memória | PROVADO |
| `bmc-k10` | `harness_bmc.c`: 10 passos desde INIT | VALIDO_NO_LIMITE |
| `kind-P5` | `harness_bmc.c -DUNBOUNDED` com k-indução | PROVADO (k=2) |
| `reach-seg1..8` | `harness_reach.c`: alcançabilidade composicional | ALCANCADO (contraexemplo = caminho) |

Logs completos e contraexemplos: `verification/results/esbmc_logs/`. Para rodar um harness à mão:

```bash
cd verification/esbmc
$ESBMC harness_step.c ../../core/fsm_core.c -I../../core --z3 -DPROP_P3 --overflow-check --unsigned-overflow-check
```

### 5.3 Tabela CVF (Q1)

```bash
python3 analysis/cvf.py                    # gera verification/results/cvf.md e cvf.json
python3 analysis/cvf.py --sufixo _mutante  # CVF dos testes negativos
```

Categorias: **comprovado** (prova para todos os contextos válidos, ou caminho encontrado),
**válido no limite** (BMC até k), **refutado** (contraexemplo) e **inconclusivo**. CVF **não** é cobertura de código.

### 5.4 Como ler os resultados no relatório

- Provas de 1 passo partem de **qualquer** contexto válido. Como o domínio é preservado (P2b), elas
  valem para qualquer execução (argumento indutivo, protocolo 6.3).
- Resultados de BMC valem **só até k**. O limite de 10 passos foi revisado para 133 nas consultas de
  alcançabilidade, porque com DT_MAX = 200 ms um ciclo completo exige 133 passos. Detalhes em `docs/registro_verificacao.md`.
- Nada disso prova o hardware, o ESP-IDF, o compilador ou o escalonador (escopo excluído 6.2).

---

## 6. Simulador — Plano B

Roda o mesmo `fsm_core.c` no computador, com o atuador emulado no terminal.

```bash
python3 tools/ptt7.py sim                        # modo interativo, tempo real
```

Teclas: `1`/`2` botões de pedestre · `a`/`b` liga/desliga perda de sinal do botão 1/2 ·
`t` falha de temporização · `r` reset · `s` status · `q` sair.

Exemplo de saída:

```
[   0.000 s] INIT                               VEIC[VM -- --]  PED[VM --]
[   2.002 s] VERDE_VEICULO                      VEIC[-- -- VD]  PED[VM --]  <- T1
  > botao pedestre 1
[   2.508 s] PEDIDO_PEDESTRE                    VEIC[-- -- VD]  PED[VM --]  <- T3
[   9.990 s] ATENCAO_VEICULO                    VEIC[-- AM --]  PED[VM --]  <- T6
```

Experimento de falhas em tempo virtual (mesmo formato CSV do firmware, `plataforma=sim`):

```bash
./build/ptt7_sim --falhas 50 > data/raw/sim_falhas.log
python3 analysis/analyze.py data/raw/sim_falhas.log --plataforma sim
```

> Pelo Plano B, os tempos do simulador **não** são atribuídos ao ESP32. Se ele for usado no lugar da
> placa, o relatório deve declarar a ausência de validação física.

---

## 7. Firmware no ESP32

### 7.1 Montar a bancada

Siga **`docs/bancada.md`**: 5 LEDs com resistores, 3 botões, 2 jumpers de supervisão
(GPIO22 e GPIO23 ao GND) e o jumper de estímulo GPIO16 → GPIO18.

### 7.2 Compilar, gravar e monitorar

No **ESP-IDF 5.1 PowerShell** (Windows):

```powershell
cd C:\project\ptt7\firmware
idf.py set-target esp32          # só na primeira vez
idf.py build
idf.py -p COM3 flash monitor     # troque COM3 pela porta da placa
```

Para sair do monitor: `Ctrl+]`.

Saída esperada ao ligar:

```
# PTT7 semaforo FSM | firmware ... | ESP-IDF v5.1.4 | compilado ...
# cpu_mhz=240 freertos_hz=1000 periodo_ms=10 debounce_us=50000 supervisao_hw=1 core_iram=sim
# T_INIT=2000 T_VERDE_MIN=8000 T_AMARELO=3000 T_VERDE_PED=6000 T_ATENCAO_PED=3000 T_RECOVERY=3000 DT_MAX=200
TR,2001234,INIT,VERDE_VEICULO,T1
```

### 7.3 Teste funcional manual (smoke test, cerca de 5 min)

Com o monitor aberto, faça e confira:

| # | Ação | Esperado nos LEDs | Esperado na serial |
|---|---|---|---|
| F1 | Ligar / reset da placa | veículo VM + pedestre VM por 2 s, depois veículo VD | `TR,...,INIT,VERDE_VEICULO,T1` |
| F2 | Esperar mais de 8 s e apertar PED1 | VD → AM (3 s) → veículo VM + pedestre VD (6 s) → pedestre VM piscando (3 s) → veículo VD | `LAT,...,VERDE_VEICULO,T4,...` e TR T4, T8, T10, T12 |
| F3 | Apertar PED2 logo depois de voltar ao verde | verde continua até completar 8 s, depois AM | `TR ... T3`, depois `T6` |
| F4 | Retirar o jumper do GPIO22 | **amarelo piscante**, todos os verdes apagados | `TR,...,VERDE_VEICULO,FALHA,T5` (ou T7/T9/... conforme o estado) |
| F5 | Apertar RESET com o jumper ainda fora | continua piscando (reset ignorado) | nada |
| F6 | Recolocar o jumper e apertar RESET | veículo VM + pedestre VM por 3 s, depois veículo VD | `T14`, depois `T15` |
| F7 | Repetir F4–F6 com o GPIO23 | idem | idem |
| F8 | Digitar `atraso 300` | amarelo piscante | `TR ... FALHA` |
| F9 | Digitar `reset` | RECOVERY → VD | `T14`, `T15` |
| F10 | Digitar `stim 1` (com jumper GPIO16 → 18) | igual a apertar PED1 | `LAT,...` |

Digite `ajuda` no monitor para ver todos os comandos.

---

## 8. Experimentos de bancada (Q2 e Q3)

Feche o `idf.py monitor` antes de capturar (a porta COM só aceita um programa por vez). Os comandos
abaixo rodam no terminal do Windows com Python (o próprio ESP-IDF PowerShell serve).

### 8.1 Q2: latência ISR → GPIO (LREC)

Protocolo: **3 sessões**, cada uma com **5 ensaios de aquecimento + 10 válidos** (30 válidos no total).
O firmware espera o verde mínimo, aplica um pulso controlado em GPIO16 → GPIO18 com defasagem
aleatória e mede da entrada da ISR até o GPIO atualizado (transição crítica T4, VV → amarelo).
Cerca de 6 min por sessão.

```powershell
cd C:\project\ptt7
python tools\capture_serial.py --porta COM3 --saida data\raw\s1_lat.log --cmd "sessao 1" --cmd "lat 10 5"
python tools\capture_serial.py --porta COM3 --saida data\raw\s2_lat.log --cmd "sessao 2" --cmd "lat 10 5"
python tools\capture_serial.py --porta COM3 --saida data\raw\s3_lat.log --cmd "sessao 3" --cmd "lat 10 5"
```

Faça as sessões em momentos diferentes (por exemplo, desconectando e reconectando a placa entre elas).
Mantenha placa, firmware, cabo e computador iguais.

### 8.2 Q3: injeção de falhas e convergência ao estado seguro

Protocolo: **50 injeções válidas por tipo**. Janela de observação: 1 s. A recuperação via reset é
medida separadamente em cada ensaio (linha `REC`).

| Tipo | Falha injetada | Comando | Tempo aproximado |
|---|---|---|---|
| 1 | perda de sinal do botão de pedestre 1 | `falha 1 50` | 7 min |
| 2 | perda de sinal do botão de pedestre 2 | `falha 2 50` | 7 min |
| 3 | perda simultânea durante VERMELHO_VEICULO_VERDE_PEDESTRE | `falha 3 50` | 15 min |
| 4 | falha de temporização (tarefa travada por 300 ms) | `falha 4 50` | 7 min |

```powershell
python tools\capture_serial.py --porta COM3 --saida data\raw\falha1.log --cmd "sessao 1" --cmd "falha 1 50"
python tools\capture_serial.py --porta COM3 --saida data\raw\falha2.log --cmd "sessao 1" --cmd "falha 2 50"
python tools\capture_serial.py --porta COM3 --saida data\raw\falha3.log --cmd "sessao 1" --cmd "falha 3 50"
python tools\capture_serial.py --porta COM3 --saida data\raw\falha4.log --cmd "sessao 1" --cmd "falha 4 50"
```

Exemplo ilustrativo das linhas geradas (os valores reais saem do seu ensaio):

```
FAULT,1,0,1,VERDE_VEICULO,48211934,6.412,1,0,esp32
REC,1,0,1,1,10.3,3012.6
```

(`conv_ms` = 6,4 ms; sucesso = 1; nenhum verde depois da FALHA; recuperação OK em 3,0 s)

### 8.3 Descartes

O firmware marca sozinho as instrumentações inválidas (`#DESCARTE,...`, por exemplo um estímulo
não detectado por jumper solto). Outros erros **comprovados** de instrumentação vão em
`data/descartes.csv`, com motivo. **Nunca** descarte falhas do sistema, respostas lentas ou ausência
de recuperação: esses são resultados. Os logs brutos ficam sempre em `data/raw/`.

---

## 9. Análise dos dados e relatório

```bash
python3 analysis/analyze.py          # lê todos os logs de data/raw/
```

Gera em `analysis/out/`:

- `resumo.md`: tabelas de Q2 (por transição e por sessão), Q3 (taxa de sucesso + n, convergência),
  recuperação, descartes e se o protocolo foi atendido (≥ 30 válidas/3 sessões; ≥ 50 injeções por tipo).
- `latencias.csv`, `falhas.csv`, `recuperacao.csv`: dados consolidados.
- `latencia_boxplot.png`, `convergencia_boxplot.png`: gráficos (precisam do matplotlib).

Estatísticas: média, mediana, desvio-padrão, IIQ (Q1–Q3), mínimo e máximo. O p99 **não** é
reportado (30 observações não bastam).

Para o relatório (OE5), use juntos: `verification/results/cvf.md` (Q1), `analysis/out/resumo.md`
(Q2/Q3), `docs/registro_verificacao.md` (contraexemplos, premissas, limites) e
`verification/results/versoes.txt` (reprodutibilidade).

### Roteiro do vídeo (3–5 min)

1. (30 s) Bancada e diagrama da FSM (`spec/fsm_spec.md`).
2. (60 s) Operação normal e pedido de pedestre (F1–F3).
3. (60 s) Falha: retirar o jumper → amarelo piscante; reset bloqueado; recuperação (F4–F6).
4. (60 s) `python3 tools/ptt7.py verify` rodando e a tabela CVF; mostrar o mutante sendo refutado.
5. (30 s) Resultados de latência e fallback (`analysis/out/resumo.md`).

---

## 10. Checklist de aceitação

| Id | Item | Como verificar | OK? |
|---|---|---|---|
| A1 | Ferramentas instaladas | `ptt7.py check` | ☐ |
| A2 | Build de host | `ptt7.py build` → `Build OK` | ☐ |
| A3 | Testes de host | `ptt7.py test` → `TESTES: OK` | ☐ |
| A4 | Z3 (modelo) | 23 consultas, 0 refutadas/inconclusivas | ☐ |
| A5 | ESBMC (código) | 16 consultas, 0 refutadas/inconclusivas | ☐ |
| A6 | Testes negativos | `z3 --mutante` e `esbmc --mutante` refutam P6 | ☐ |
| A7 | CVF | `cvf.md`: P1–P6 comprovadas nas duas camadas | ☐ |
| A8 | Firmware compila | `idf.py build` sem erros | ☐ |
| A9 | Smoke test | F1–F10 da seção 7.3 | ☐ |
| A10 | Q2 coletado | ≥ 30 LAT válidas de T4 em 3 sessões | ☐ |
| A11 | Q3 coletado | ≥ 50 injeções válidas por tipo (1–3) | ☐ |
| A12 | Análise | `analysis/out/resumo.md` com "ATENDIDO" | ☐ |
| A13 | Versões registradas | `ptt7.py versions` + commit do núcleo | ☐ |
| A14 | Repositório | `git init`, commit inicial, LICENSE presente | ☐ |

Versionamento (uma vez):

```bash
cd /mnt/c/project/ptt7
git init && git add . && git commit -m "PTT7: núcleo FSM, verificação em duas camadas, firmware e manual"
```

Registre o hash do commit do núcleo verificado no relatório (`git rev-parse --short HEAD`).

---

## 11. Solução de problemas

| Sintoma | Causa provável | Solução |
|---|---|---|
| `Z3 (API C++) nao encontrado` | `z3-solver` não instalado no Python em uso | ative o venv e rode `python -m pip install z3-solver` |
| `error while loading shared libraries: libz3.so` | venv mudou de lugar | `python3 tools/ptt7.py build` de novo |
| `esbmc nao encontrado` | variável `ESBMC` ausente | `export ESBMC=/caminho/esbmc` (seção 2.2) |
| `reach-seg3` demora | segmento de 41 passos | normal (cerca de 1 min); use `--rapido` no dia a dia |
| Placa liga direto em amarelo piscante | jumpers GPIO22/23 ao GND ausentes | coloque os jumpers (ou compile com `-DPTT7_SUPERVISAO_HW=0`) |
| `#DESCARTE ... estimulo nao detectado` | jumper GPIO16 → GPIO18 ausente | ligue o jumper e repita a sessão |
| `capture_serial.py` não abre a porta | `idf.py monitor` aberto | feche o monitor (`Ctrl+]`) |
| Porta COM não aparece | driver CP210x/CH340 ausente ou cabo só de energia | instale o driver; troque o cabo |
| No WSL o `build` fica lento em `/mnt/c` | E/S entre Windows e Linux | normal; ou copie o projeto para `~/ptt7` |
