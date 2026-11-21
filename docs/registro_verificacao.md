# Registro da verificação em duas camadas (OE3)

Diário de premissas, limites de exploração, contraexemplos e correções. Atualize a cada execução
relevante (data, commit, ferramenta, resultado).

## Premissas de execução

| Id | Premissa | Onde é usada |
|---|---|---|
| A1 | O invólucro chama `fsm_step()` com `dt_ms` = tempo real decorrido desde o passo anterior | todas as propriedades temporais |
| A2 | `dt_ms > DT_MAX (200 ms)` é falha de temporização (tarefa atrasada/travada) | P4, P5 |
| A3 | Supervisão de linha: `sigX_ok = 0` representa perda do sinal do botão X | P4, P5 |
| A4 | Domínio válido do contexto: estado ∈ [0,7], `pending` ∈ {0,1}, temporizadores ≤ 60 000 ms | provas de 1 passo (indutivas) |
| A5 | Entradas booleanas ∈ {0,1}; `dt_ms` é qualquer `uint32` | ESBMC e Z3 |
| A6 | Progresso: para alcançabilidade e P2c usam-se entradas sem falha e `dt = DT_MAX` | P1, P2c |
| A7 | Hardware, compilador, ESP-IDF e escalonador **não** são verificados formalmente | escopo excluído (6.2) |

## Limites de exploração adotados

- **Protocolo:** limite inicial de 10 passos. Ele vale para P5 (BMC k=10 nas duas camadas).
- **Revisão após análise dos temporizadores:** com `DT_MAX = 200 ms`, o ciclo completo exige
  ⌈T/DT_MAX⌉ passos por estado temporizado, ou seja 133 passos no total (inclui folga de 8).
  Portanto k=10 **não** basta para alcançabilidade. O Z3 usa k_ciclo = 133 para P1 e P2c.
- **ESBMC:** o BMC monolítico de 133 passos a partir de INIT não terminou em 110 s. A alcançabilidade
  foi feita de forma **composicional** em 8 segmentos (`harness_reach.c`), cada um partindo do
  contexto exato de entrada no estado de origem. O maior segmento tem 41 passos (~1 min).
- **Provas ilimitadas:** P2, P3, P4 e P6 são provadas em 1 passo sobre **contexto arbitrário
  válido** com preservação do domínio (indução). P5 tem prova por k-indução no ESBMC (k=2) e
  passo indutivo no Z3 (`P5i`), complementando o BMC k=10.

## Achados (contraexemplos) e correções

### ESBMC-01: overflow aritmético em `sat_add` (corrigido)

- **Data:** 06/10/2026 · **Ferramenta:** ESBMC 8.5.0, `--unsigned-overflow-check` · **Harness:** `harness_step.c`
- **Contraexemplo:** contexto `{FALHA, elapsed=0, green=0, pending=1}` com `dt_ms` grande:
  `a + b` em `uint32` dá a volta (*wrap-around*) antes da saturação.
- **Análise:** o resultado funcional estava correto, porque o teste `r < a` detectava o *wrap*.
  Mesmo assim o código dependia de *overflow* aritmético, o que é proibido por regras de codificação
  segura (CWE-190) e torna a semântica dependente do tipo.
- **Correção:** `sat_add` passou a testar `b >= SAT - a` antes de somar. Re-verificação: SUCCESSFUL.
- **Lição para o relatório:** a camada de código encontrou um defeito que o modelo Z3 (aritmética
  de inteiros ilimitados) **não pode** encontrar. É um exemplo concreto do valor da segunda camada (Q1).

### Teste negativo (mutante `FSM_INJECT_BUG`)

O mutante faz `FALHA → VERMELHO_VEICULO_VERDE_PEDESTRE` no reset. Resultado esperado e obtido:

- Z3 (`--mutante`): **P6 REFUTADO** com contraexemplo; P1.7 (RECOVERY) passa a não ser alcançado.
- ESBMC (`--mutante`): `step-P6`, `step-mem`, `bmc-k10` e `kind-P5` **REFUTADO**; `reach-seg8` INCONCLUSIVO.
- Testes unitários e conformidade: falham/divergem.

Conclusão: os procedimentos têm poder de detecção para a violação de P6.

## Histórico de execuções

| Data | Commit | Z3 | ESBMC | Resultado |
|---|---|---|---|---|
| 06/10/2026 | (inicial) | 5.1.0 | 8.5.0 | Modelo e código: P1–P6 comprovadas (ver `verification/results/cvf.md`) |
| 21/11/2026 | 412b609 (núcleo) / b4fcf0e (HEAD) | 5.4.1 | 8.5.0 | Reexecução: P1–P6 comprovadas nas duas camadas; mutante refuta P6 nas duas; 336 verificações unitárias e 0 divergências de conformidade |
| 21/11/2026 | 412b609 (núcleo) | 5.4.1 | 8.7.0 | Teste de sensibilidade à versão: `kind-P5` INCONCLUSIVO (demais iguais) — ver seção abaixo |

## Reexecução de 21/11/2026 e sensibilidade à versão do ESBMC

- **Objetivo:** reconfirmar P1–P6 sobre o núcleo do commit `412b609` antes da entrega e do design
  review. SHA-256 dos arquivos verificados em `verification/results/versoes.txt`.
- **Ambiente:** Linux x86_64, gcc 13.3, Python 3.13, `z3-solver` 5.4.1 (o registro de 06/10 usou
  5.1.0), ESBMC 8.5.0 (mesma versão de 06/10).
- **Resultado:** Z3 com 23 consultas e 0 refutadas/inconclusivas; ESBMC com 16 consultas e 0
  refutadas/inconclusivas; CVF-Modelo = CVF-Código = 6/6 comprovadas; testes unitários com 336
  verificações e 0 falhas; conformidade com 16/16 transições e 0 divergências. O mutante
  `FSM_INJECT_BUG` continua refutando P6 no Z3 e em step-P6, step-mem, bmc-k10 e kind-P5 no ESBMC.
- **Achado — sensibilidade à versão:** com o ESBMC **8.7.0** (versão mais recente), a consulta
  `kind-P5` (k-indução de P5) terminou com `VERIFICATION UNKNOWN`: o passo indutivo não fechou até
  k = 10. Com o 8.5.0 a mesma consulta prova P5 em k = 2. As outras 15 consultas deram o mesmo
  resultado nas duas versões. Saída preservada em `verification/results/reexecucao_esbmc_8.7.0/`.
- **Leitura correta:** não é contraexemplo (nada foi refutado). P5 continua sustentada por três
  evidências que não dependem do 8.7.0: passo indutivo `P5i` no Z3, BMC k = 10 nas duas camadas e
  P4 + P6 provadas em 1 passo sobre contexto arbitrário. Em 8.7.0 o CVF-Código seria 5 comprovadas
  e 1 inconclusiva (P5); o protocolo manda registrar exatamente isso, sem alegar prova completa.
- **Decisão:** fixar o ESBMC em **8.5.0** para a entrega (o contrato manda fixar as versões das
  ferramentas, item 7) e citar a sensibilidade à versão no relatório, em "Limitações".

## Cenários de aceitação acrescentados em 21/11/2026

Os cenários 01–07 do simulador cobriam 11 das 16 transições. Foram criados 08–12 para cobrir as
falhas em INIT (T2), PEDIDO_PEDESTRE (T7), ATENCAO_VEICULO (T9), ATENCAO_PEDESTRE (T13) e
RECOVERY (T16). Com isso, os 12 cenários cobrem T1–T16 (62 verificações, 0 falhas). Registro:
`docs/validacao/evidencias/cenarios_sim.txt`. Matriz completa: `docs/validacao/matriz_rastreabilidade.md`.
