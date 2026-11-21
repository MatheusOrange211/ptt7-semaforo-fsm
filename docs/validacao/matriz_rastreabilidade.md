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

## Como ler

Exemplo simples: a transição **T4** diz "do verde dos veículos, com pedido de travessia e
verde mínimo cumprido, vá para o amarelo". Para considerá-la validada, queremos ver esse
comportamento em vários lugares independentes: no teste unitário, no modelo Z3 (mesmos
vetores), em um cenário do simulador e, por fim, nos LEDs da bancada.

Legenda: ✅ evidência existente e conferida em 21/11/2026 · ⏳ pendente (depende da placa) ·
— não se aplica.

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

| T | Origem → destino | Guarda (resumo) | Unitário | Conformidade | Cenário (sim) | Bancada |
|---|---|---|---|---|---|---|
| T1 | INIT → VERDE_VEICULO | sem falha e 2 s | ✅ | ✅ | ✅ 01 | ⏳ F1 |
| T2 | INIT → FALHA | falha | ✅ | ✅ | ✅ 08 | ⏳ B1 |
| T3 | VERDE_VEICULO → PEDIDO_PEDESTRE | pedido e verde < 8 s | ✅ | ✅ | ✅ 02, 09 | ⏳ F3 |
| T4 | VERDE_VEICULO → ATENCAO_VEICULO | pedido e verde ≥ 8 s | ✅ | ✅ | ✅ 01, 05, 10, 11 | ⏳ F2 (e Q2) |
| T5 | VERDE_VEICULO → FALHA | falha | ✅ | ✅ | ✅ 03, 04, 06, 12 | ⏳ F4, F7, F8 |
| T6 | PEDIDO_PEDESTRE → ATENCAO_VEICULO | verde ≥ 8 s | ✅ | ✅ | ✅ 02 | ⏳ F3 |
| T7 | PEDIDO_PEDESTRE → FALHA | falha | ✅ | ✅ | ✅ 09 | ⏳ B2 |
| T8 | ATENCAO_VEICULO → VERMELHO_VEICULO_VERDE_PEDESTRE | 3 s de amarelo | ✅ | ✅ | ✅ 01, 05, 11 | ⏳ F2 |
| T9 | ATENCAO_VEICULO → FALHA | falha | ✅ | ✅ | ✅ 10 | ⏳ B3 |
| T10 | VERMELHO_VEICULO_VERDE_PEDESTRE → ATENCAO_PEDESTRE | 6 s de verde de pedestre | ✅ | ✅ | ✅ 01, 11 | ⏳ F2 |
| T11 | VERMELHO_VEICULO_VERDE_PEDESTRE → FALHA | falha | ✅ | ✅ | ✅ 05 | ⏳ Q3 tipo 3 |
| T12 | ATENCAO_PEDESTRE → VERDE_VEICULO | 3 s de atenção | ✅ | ✅ | ✅ 01 | ⏳ F2 |
| T13 | ATENCAO_PEDESTRE → FALHA | falha | ✅ | ✅ | ✅ 11 | ⏳ B4 |
| T14 | FALHA → RECOVERY | reset e sem falha | ✅ | ✅ | ✅ 03–06, 08–12 | ⏳ F6, F9 |
| T15 | RECOVERY → VERDE_VEICULO | 3 s sem falha | ✅ | ✅ | ✅ 03–06, 08–12 | ⏳ F6, F9 |
| T16 | RECOVERY → FALHA | falha | ✅ | ✅ | ✅ 12 | ⏳ B5 |

**Resultado de host:** as 16 transições têm evidência em três fontes independentes (teste
unitário, conformidade com o modelo e cenário de aceitação). Até 20/11, os cenários cobriam
11 das 16 transições. Em 21/11 foram criados os cenários 08–12, que cobrem T2, T7, T9, T13 e T16
(todos aprovados, 62 verificações e 0 falhas nos 12 cenários).

### Itens complementares de bancada (B1–B5)

O roteiro F1–F10 do manual não força falha em todos os estados. Para fechar a coluna de
bancada, acrescentar ao teste funcional:

| Item | Ação | Esperado nos LEDs | Esperado na serial |
|---|---|---|---|
| B1 | Ligar a placa **sem** o jumper do GPIO22 | amarelo piscante desde a partida | `TR,...,INIT,FALHA,T2` |
| B2 | Apertar PED1 logo após o verde começar e, antes de 8 s, retirar o jumper do GPIO22 | amarelo piscante | `...,PEDIDO_PEDESTRE,FALHA,T7` |
| B3 | Durante o amarelo veicular, retirar o jumper do GPIO23 | amarelo piscante | `...,ATENCAO_VEICULO,FALHA,T9` |
| B4 | Durante o vermelho piscante de pedestre, retirar o jumper do GPIO22 | amarelo piscante | `...,ATENCAO_PEDESTRE,FALHA,T13` |
| B5 | Em FALHA, recolocar o jumper, apertar RESET e, nos 3 s de RECOVERY, retirar o jumper de novo | volta ao amarelo piscante | `T14`, depois `...,RECOVERY,FALHA,T16` |

Registre cada item com foto/vídeo curto ou com a linha `TR` capturada (a captura serial vai para
`data/raw/`). Isso serve também de material para o vídeo de demonstração.

## 2. Propriedades (P1–P6) × evidências

Resultado da reexecução de 21/11/2026 (Z3 5.4.1; ESBMC 8.5.0, a mesma versão do registro de
06/10). Detalhes em `docs/registro_verificacao.md`.

| Propriedade (contrato 6.3) | Modelo — Z3 | Código — ESBMC | Teste / cenário | Bancada |
|---|---|---|---|---|
| P1 alcançabilidade | ✅ P1.0–P1.7 (k_ciclo = 133) | ✅ reach-seg1…8 | cenários 01–12 visitam os 8 estados | ⏳ F1–F6 |
| P2 ausência de deadlock | ✅ P2a, P2b, P2c.* | ✅ step-P2, step-mem | unitário (estado corrompido → FALHA) | — |
| P3 exclusão mútua (+ verde mínimo) | ✅ P3, P3b | ✅ step-P3, step-P3b | cenários 01 e 02 (verde de pedestre só em VP; pedido antecipado espera os 8 s) | ⏳ F2, F3 |
| P4 violação → estado seguro | ✅ P4 | ✅ step-P4 | cenários 03–06, 08–12 | ⏳ Q3 (tipos 1–4) |
| P5 convergência do fallback | ✅ P5i (indutivo) + P5 (BMC k = 10) | ✅ kind-P5 (k = 2) + bmc-k10 | cenários 03, 12 (falha persiste → fica em FALHA) | ⏳ Q3 (taxa e tempo) |
| P6 FALHA não volta direto a VP | ✅ P6 | ✅ step-P6 | cenários com `espera RECOVERY` | ⏳ F6, B5 |

**Poder de detecção (validação negativa):** com o mutante `-DFSM_INJECT_BUG`, o Z3 refuta P6 e o
ESBMC refuta step-P6, step-mem, bmc-k10 e kind-P5 (reexecutado em 21/11/2026).

## 3. Perguntas do contrato × evidências

| Pergunta | Evidência | Situação |
|---|---|---|
| Q1 — modelo e código concordam? | `verification/results/cvf.md` | ✅ 6/6 propriedades concordantes (ESBMC 8.5.0) |
| Q2 — latência ISR → GPIO | `data/raw/s1..s3_lat.log` → `analysis/analyze.py` | ⏳ bancada (30 medições em 3 sessões) |
| Q3 — taxa e tempo do fallback | `data/raw/falha1..4.log` → `analysis/analyze.py` | ⏳ bancada (50 injeções por tipo). Plano B pronto: `data/raw/sim_falhas.log` (200/200 em tempo virtual, **não** atribuível ao ESP32) |

## 4. O que falta para fechar a matriz

1. Coluna **Bancada**: executar F1–F10 e B1–B5 e anexar as linhas `TR` capturadas.
2. Q2 e Q3: rodar os comandos da seção 8 do `MANUAL_EXECUCAO.md`.
3. Se a versão atual do firmware tiver mudado o `core/`, refazer `python3 tools/ptt7.py all` e
   atualizar o commit e o hash no topo deste documento (ver `docs/validacao/alinhamento_modelo.md`).
