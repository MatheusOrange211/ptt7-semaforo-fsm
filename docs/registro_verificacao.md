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
