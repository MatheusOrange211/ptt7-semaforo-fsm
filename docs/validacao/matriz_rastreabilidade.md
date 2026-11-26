# Matriz de rastreabilidade — validação da máquina de estados (PTT7)

Esta matriz liga cada transição e cada propriedade da FSM à evidência que comprova o seu
comportamento. Ela responde à pergunta "a máquina que roda é a máquina especificada?".

- **Fundamento no Catálogo PTT7:** artefatos exigidos = firmware, modelo formal da FSM,
  saída da verificação, evidência de teste em bancada e relatório.
- **Fundamento no contrato (item 6.3, "principal ameaça e mitigação"):** tabela de transições
  comum como especificação, revisão cruzada, testes com os mesmos vetores de entrada e
  identificação do commit do núcleo verificado.

**Versão do núcleo rastreada:** commit `412b609` (último que alterou `core/`).
SHA-256 de `core/fsm_core.c`: `80439f7c…58cf93` (valor completo em `verification/results/versoes.txt`).
**Data da última execução das evidências de host:** 21/11/2026.
**Data da bancada física (ESP32):** 24/11/2026 (resultados em `docs/validacao/evidencias/resultados_bancada_2026-11-24.md`).

## Como ler

Exemplo simples: a transição **T4** diz "do verde dos veículos, com pedido de travessia e
verde mínimo cumprido, vá para o amarelo". Para considerá-la validada, queremos ver esse
comportamento em vários lugares independentes: no teste unitário, no modelo Z3 (mesmos
vetores), em um cenário do simulador e, por fim, nos LEDs da bancada.

Legenda: ✅ evidência existente e conferida (host em 21/11/2026; logs de bancada de 24/11/2026,
conferidos em 25/11/2026) · ⚠️ evidência parcial ou exploratória · 🚫 não aplicável com
`PTT7_SUPERVISAO_HW 0` · ⏳ pendente · — não se aplica.

**Nota sobre a bancada de 24/11/2026:** o firmware rodou com `PTT7_SUPERVISAO_HW = 0` e a pinagem
adaptada; os GPIO 22/23 (SIG1/SIG2) são ignorados. A perda de sinal foi **injetada por software**
(comandos `falha`/`perda`/`restaura`) e a falha de temporização por atraso injetado. O roteiro
F1–F10 não foi executado como teste funcional formal. Mesmo assim, os logs registram cada
transição ocorrida (linhas `TR`), inclusive as do ciclo normal. Por isso a coluna de bancada recebe ✅
sempre que há uma linha `TR` da transição em `data/raw/`, com arquivo e linha indicados.

## 1. Transições (T1–T16) × evidências

Fontes das colunas:
- **Unitário:** `tests/test_fsm_core.c` (336 verificações, 0 falhas).
- **Conformidade:** `verification/z3/conformance.cpp`, mesmos vetores aplicados ao C e ao modelo
  Z3 (semente 2026; `verification/results/conformance_vectors.csv`): 16/16 transições cobertas,
  0 divergências.
- **Cenário (sim):** `sim/cenarios/*.txt`, executados no simulador de host com o mesmo
  `fsm_core.c` (registro completo em `docs/validacao/evidencias/cenarios_sim.txt`).
- **Bancada:** roteiro do teste funcional (MANUAL_EXECUCAO.md, seção 7.3, itens F1–F10) e itens
  complementares B1–B5 definidos abaixo.

| T | Origem → destino | Guarda (resumo) | Unitário | Conformidade | Cenário (sim) | Bancada (ESP32, logs em `data/raw/`) |
|---|---|---|---|---|---|---|
| T1 | INIT → VERDE_VEICULO | sem falha e 2 s | ✅ | ✅ | ✅ 01 | ⏳ sem registro (capturas de 24/11 começaram com a placa ligada); previsto nos `reteste_s*_boot.log` |
| T2 | INIT → FALHA | falha | ✅ | ✅ | ✅ 08 | 🚫 sem registro; B1 exige `PTT7_SUPERVISAO_HW 1` |
| T3 | VERDE_VEICULO → PEDIDO_PEDESTRE | pedido e verde < 8 s | ✅ | ✅ | ✅ 02, 09 | ✅ `falha1.log` (6; linha 13), `falha2.log` (1; linha 12), `s1_lat.log` (2; linha 15) — pedidos não comandados |
| T4 | VERDE_VEICULO → ATENCAO_VEICULO | pedido e verde ≥ 8 s | ✅ | ✅ | ✅ 01, 05, 10, 11 | ✅ `falha3.log` (50; linha 7), `s1_lat.log` (2; linha 9); latência da Q2 em reteste |
| T5 | VERDE_VEICULO → FALHA | falha | ✅ | ✅ | ✅ 03, 04, 06, 12 | ✅ `falha1.log` (44; linha 7), `falha2.log` (49; linha 7), `falha4.log` (50; linha 7) |
| T6 | PEDIDO_PEDESTRE → ATENCAO_VEICULO | verde ≥ 8 s | ✅ | ✅ | ✅ 02 | ✅ `s1_lat.log` (2; linha 17) |
| T7 | PEDIDO_PEDESTRE → FALHA | falha | ✅ | ✅ | ✅ 09 | ✅ `falha1.log` (6; linha 14), `falha2.log` (1; linha 14) |
| T8 | ATENCAO_VEICULO → VERMELHO_VEICULO_VERDE_PEDESTRE | 3 s de amarelo | ✅ | ✅ | ✅ 01, 05, 11 | ✅ `falha3.log` (50; linha 8), `s1_lat.log` (2; linha 12); amarelo medido: mediana 3000,4 ms (n = 52) |
| T9 | ATENCAO_VEICULO → FALHA | falha | ✅ | ✅ | ✅ 10 | ⏳ sem registro; equivalente por software: `perda 1` durante o amarelo |
| T10 | VERMELHO_VEICULO_VERDE_PEDESTRE → ATENCAO_PEDESTRE | 6 s de verde de pedestre | ✅ | ✅ | ✅ 01, 11 | ✅ `s1_lat.log` (2; linha 13); verde de pedestre medido: 6000,0 ms (n = 2) |
| T11 | VERMELHO_VEICULO_VERDE_PEDESTRE → FALHA | falha | ✅ | ✅ | ✅ 05 | ✅ `falha3.log` (50; linha 9) |
| T12 | ATENCAO_PEDESTRE → VERDE_VEICULO | 3 s de atenção | ✅ | ✅ | ✅ 01 | ✅ `s1_lat.log` (2; linha 14); atenção medida: 3000–3009 ms (n = 2) |
| T13 | ATENCAO_PEDESTRE → FALHA | falha | ✅ | ✅ | ✅ 11 | ⏳ sem registro; equivalente por software: `perda 1` durante o vermelho piscante de pedestre |
| T14 | FALHA → RECOVERY | reset e sem falha | ✅ | ✅ | ✅ 03–06, 08–12 | ✅ `falha1.log`–`falha4.log` (50 cada; primeira em `falha1.log`, linha 9) |
| T15 | RECOVERY → VERDE_VEICULO | 3 s sem falha | ✅ | ✅ | ✅ 03–06, 08–12 | ✅ `falha1.log`–`falha4.log` (50 cada; primeira em `falha1.log`, linha 11); RECOVERY medido: mediana 3000,4 ms (n = 200) |
| T16 | RECOVERY → FALHA | falha | ✅ | ✅ | ✅ 12 | ⏳ sem registro; equivalente por software: `perda 1` nos 3 s de RECOVERY |

**Resultado de bancada:** 11 das 16 transições têm registro `TR` nos logs físicos de 24/11/2026
(T3–T8, T10–T12, T14 e T15). As linhas citadas são a primeira ocorrência em cada arquivo, e o número
entre parênteses é o total de ocorrências. T1, T2, T9, T13 e T16 ainda não têm registro físico; a
evidência delas está nas três colunas de computador. A contagem pode ser refeita com
`grep -c "^TR,.*,T4$" data/raw/falha3.log` (trocando a transição e o arquivo).

**Resultado de host:** as 16 transições têm evidência em três fontes independentes (teste
unitário, conformidade com o modelo e cenário de aceitação). Até 20/11, os cenários cobriam
11 das 16 transições. Em 21/11 foram criados os cenários 08–12, que cobrem T2, T7, T9, T13 e T16
(todos aprovados, 62 verificações e 0 falhas nos 12 cenários).

### Itens complementares de bancada (B1–B5)

O roteiro F1–F10 do manual não força falha em todos os estados. Para fechar a coluna de
bancada, acrescentar ao teste funcional (itens escritos para os jumpers dos GPIO 22/23; **não
executados na bancada de 24/11/2026**, onde `HW = 0` ignora esses pinos; coluna final indica o
equivalente por injeção de software, quando existe):

| Item | Ação | Esperado nos LEDs | Esperado na serial | Situação em 24/11/2026 |
|---|---|---|---|---|
| B1 | Ligar a placa **sem** o jumper do GPIO22 | amarelo piscante desde a partida | `TR,...,INIT,FALHA,T2` | 🚫 não executado (HW=0); sem equivalente por software |
| B2 | Apertar PED1 logo após o verde começar e, antes de 8 s, retirar o jumper do GPIO22 | amarelo piscante | `...,PEDIDO_PEDESTRE,FALHA,T7` | 🚫 por jumper; equivalente por software (`perda 1`/`perda 2`) ocorreu em 6 + 1 injeções de Q3 em PEDIDO_PEDESTRE |
| B3 | Durante o amarelo veicular, retirar o jumper do GPIO23 | amarelo piscante | `...,ATENCAO_VEICULO,FALHA,T9` | 🚫 não executado; sem equivalente registrado |
| B4 | Durante o vermelho piscante de pedestre, retirar o jumper do GPIO22 | amarelo piscante | `...,ATENCAO_PEDESTRE,FALHA,T13` | 🚫 não executado; sem equivalente registrado |
| B5 | Em FALHA, recolocar o jumper, apertar RESET e, nos 3 s de RECOVERY, retirar o jumper de novo | volta ao amarelo piscante | `T14`, depois `...,RECOVERY,FALHA,T16` | 🚫 não executado; `restaura` + reset por software validou T14/T15 (200/200), mas não T16 |

Registre cada item com foto/vídeo curto ou com a linha `TR` capturada (a captura serial vai para
`data/raw/`). Isso serve também de material para o vídeo de demonstração. Para executá-los de
verdade é preciso religar `PTT7_SUPERVISAO_HW = 1` ou criar os equivalentes por comando de software.

## 2. Propriedades (P1–P6) × evidências

Resultado da reexecução de 21/11/2026 (Z3 5.4.1; ESBMC 8.5.0, a mesma versão do registro de
06/10). Detalhes em `docs/registro_verificacao.md`.

| Propriedade (contrato 6.3) | Modelo — Z3 | Código — ESBMC | Teste / cenário | Bancada |
|---|---|---|---|---|
| P1 alcançabilidade | ✅ P1.0–P1.7 (k_ciclo = 133) | ✅ reach-seg1…8 | cenários 01–12 visitam os 8 estados | ⚠️ 7 de 8 estados visitados nos logs (VV, PP, AV, VP, AP, FALHA, RECOVERY); INIT só aparece no *banner*, ainda não capturado |
| P2 ausência de deadlock | ✅ P2a, P2b, P2c.* | ✅ step-P2, step-mem | unitário (estado corrompido → FALHA) | — |
| P3 exclusão mútua (+ verde mínimo) | ✅ P3, P3b | ✅ step-P3, step-P3b | cenários 01 e 02 (verde de pedestre só em VP; pedido antecipado espera os 8 s) | ⚠️ P3b: em `falha3.log`, verde de veículos de 8104,6 ms antes de cada T4 (n = 49); P3 não é observável no log (só os LEDs mostram) |
| P4 violação → estado seguro | ✅ P4 | ✅ step-P4 | cenários 03–06, 08–12 | ✅ Q3 (tipos 1–4): verde_apos_falha = 0 em 200/200 injeções (por software) |
| P5 convergência do fallback | ✅ P5i (indutivo) + P5 (BMC k = 10) | ✅ kind-P5 (k = 2) + bmc-k10 | cenários 03, 12 (falha persiste → fica em FALHA) | ✅ Q3: 200/200, convergência 0,42–9,43 ms (tipos 1–3) e ~305 ms (tipo 4) |
| P6 FALHA não volta direto a VP | ✅ P6 | ✅ step-P6 | cenários com `espera RECOVERY` | ⚠️ recuperação via reset 200/200 em Q3; F6 e B5 não executados |

**Poder de detecção (validação negativa):** com o mutante `-DFSM_INJECT_BUG`, o Z3 refuta P6 e o
ESBMC refuta step-P6, step-mem, bmc-k10 e kind-P5 (reexecutado em 21/11/2026).

## 3. Perguntas do contrato × evidências

| Pergunta | Evidência | Situação |
|---|---|---|
| Q1 — modelo e código concordam? | `verification/results/cvf.md` | ✅ 6/6 propriedades concordantes (ESBMC 8.5.0) |
| Q2 — latência ISR → GPIO | `data/raw/reteste_s1..s3_lat.log` → `analysis/analyze.py` | ⚠️ **protocolo não atendido em 24/11/2026** (0 medições válidas, 1 sessão; `s1_lat.log`). Exploratório: 23 registros LAT reais, mediana 14 µs (ISR → GPIO). Reteste pelo MANUAL, seção 8.1 |
| Q3 — taxa e tempo do fallback | `data/raw/falha1..4.log` → `analysis/analyze.py` | ✅ protocolo atendido no ESP32 físico (24/11/2026): 4 × 50 = 200/200, falhas injetadas por software (`HW = 0`). Plano B (`data/raw/sim_falhas.log`, tempo virtual) fica só como referência |

## 4. O que falta para fechar a matriz

1. **Q2:** refazer conforme o protocolo (3 sessões, 30 medições válidas de T4) pelo roteiro do
   `MANUAL_EXECUCAO.md`, seção 8.1. O registro do *banner* em cada sessão também fornece a T1.
2. Coluna **Bancada** (opcional): para T9, T13 e T16, digitar `perda 1` com o monitor aberto
   durante o amarelo, durante o vermelho piscante de pedestre e durante os 3 s de RECOVERY,
   capturando a serial em `data/raw/`. T2 só pode ser exercitada com `PTT7_SUPERVISAO_HW 1` (B1).
3. Se a versão atual do firmware tiver mudado o `core/`, refazer `python3 tools/ptt7.py all` e
   atualizar o commit e o hash no topo deste documento (ver `docs/validacao/alinhamento_modelo.md`).
   Na bancada de 24/11/2026 o núcleo tinha os mesmos SHA-256 de `verification/results/versoes.txt`.
