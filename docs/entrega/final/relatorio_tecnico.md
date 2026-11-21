# Relatório técnico conclusivo — PTT7

**Controle de semáforo com travessia de pedestres baseado em máquina de estados no ESP32 com
verificação em duas camadas e avaliação da latência de resposta**

Pedro Vinicius da Silva Ribeiro · Matheus Naranjo Corrêa — Orientador: Prof. Herbert Rocha
Arquitetura e Organização de Computadores · PPCA/UFRR · 2026.2 · Entrega: 26/11/2026

> **Estado deste documento (21/11/2026):** as seções 1–5 e 7 estão redigidas com os resultados
> já obtidos. Os trechos marcados com `[PREENCHER: …]` dependem dos dados de bancada (Q2 e Q3) e
> devem ser completados depois de rodar `python analysis/analyze.py`. A estrutura segue o
> contrato: item 10.1 (resultados por propriedade, medições, contraexemplos, limitações e
> evidências dos testes) e item 6.1 (evidência de cada objetivo específico).

---

## Resumo

Este relatório apresenta um controlador de semáforo com travessia de pedestres modelado como
máquina de estados finitos (FSM) de 8 estados e 16 transições, implementado no ESP32 com o núcleo
lógico isolado em C. Seis propriedades de segurança e vivacidade (P1–P6) foram verificadas em duas
camadas: o modelo abstrato com Z3 e o próprio código C com ESBMC. Todas foram comprovadas nas duas
camadas, com poder de detecção demonstrado por um mutante. A camada de código encontrou um defeito
de *overflow* invisível ao modelo. [PREENCHER: uma frase com a latência mediana de T4 e a taxa de
sucesso do fallback, indicando se os dados vêm do ESP32 ou do Plano B.]

## 1. Introdução e problema

Em um semáforo, um erro de lógica pode autorizar verde simultâneo para veículos e pedestres.
O problema de pesquisa (contrato, item 2.2) pergunta de que forma a verificação formal em duas
camadas demonstra a preservação de propriedades de segurança diante de interações assíncronas e
falhas de temporização, e qual o impacto disso na latência e na robustez em bancada.

Perguntas norteadoras: **Q1** — modelo e código concordam nas propriedades? **Q2** — qual é a
latência entre o evento do botão e o comando de transição para o amarelo? **Q3** — com que
frequência e em quanto tempo o sistema atinge o estado seguro sob falhas?

## 2. Especificação da FSM (OE1)

Fonte única: `spec/transition_table.csv` e `spec/fsm_spec.md`.

- **Estados (8):** INIT, VERDE_VEICULO, PEDIDO_PEDESTRE, ATENCAO_VEICULO,
  VERMELHO_VEICULO_VERDE_PEDESTRE, ATENCAO_PEDESTRE, FALHA (amarelo piscante) e RECOVERY.
- **Entradas:** `req1`, `req2` (botões de pedestre), `sig1_ok`, `sig2_ok` (supervisão de linha),
  `reset` e `dt_ms` (tempo decorrido, entrada explícita).
- **Falha:** `!sig1_ok || !sig2_ok || dt_ms > 200 ms`, com prioridade em todos os estados.
- **Temporizações:** INIT 2 s, verde mínimo 8 s, amarelo 3 s, verde de pedestre 6 s, atenção de
  pedestre 3 s, recovery 3 s.
- **Transições:** T1–T16 (diagrama em `spec/fsm_spec.md`, seção 3).

[INSERIR FIGURA: diagrama de estados exportado do bloco Mermaid de `spec/fsm_spec.md`.]

| Id | Propriedade (contrato 6.3) |
|---|---|
| P1 | alcançabilidade dos estados previstos |
| P2 | ausência de deadlock, com transição definida para toda entrada admissível |
| P3 | verde autorizado apenas com exclusão mútua veículo × pedestre (+ P3b: verde mínimo) |
| P4 | violação das entradas conduz ao estado seguro |
| P5 | convergência do fallback sob premissas de progresso |
| P6 | FALHA não retorna direto a VERMELHO_VEICULO_VERDE_PEDESTRE sem passar por RECOVERY |

## 3. Implementação (OE2)

- **Núcleo** (`core/fsm_core.c`): C99 puro, sem hardware, sem alocação dinâmica, temporizadores
  saturados em 60 s. O mesmo arquivo é compilado no firmware, nos testes, no simulador e no ESBMC.
- **Firmware** (`firmware/`, ESP-IDF v5.1): ISR dos botões em IRAM, notificação da tarefa de
  controle via FreeRTOS, passo periódico de 10 ms, núcleo posicionado em IRAM, instrumentação com
  `esp_timer_get_time()` e console de experimentos. Os logs saem por fila numa tarefa de baixa
  prioridade, fora do trecho cronometrado.
- **Bancada:** ESP32 DevKit V1, 5 LEDs com resistores, 3 botões e 2 jumpers de supervisão
  (`docs/bancada.md`).
- **Vínculo com a ementa:** unidade de controle como FSM; E/S e interrupções; latência de
  tratamento (ISR → decisão → GPIO); memória (código crítico em IRAM).

Teste funcional do núcleo: `tests/test_fsm_core.c`, com 336 verificações e 0 falhas (21/11/2026).

## 4. Verificação em duas camadas (OE3, Q1)

### 4.1 Procedimento

- **Camada 1 — modelo (Z3 5.4.1, API C++):** 23 consultas. As de segurança (P2–P4, P6) são provas
  de 1 passo sobre contexto arbitrário válido, com preservação de domínio (argumento indutivo).
  P1 e P2c usam BMC existencial com k_ciclo = 133. P5 combina BMC k = 10 e passo indutivo `P5i`.
- **Camada 2 — código (ESBMC 8.5.0, backend Z3):** 16 consultas sobre o mesmo `fsm_core.c`. Usa
  harness de 1 passo (P2, P3, P3b, P4, P6, memória e *overflow*), BMC k = 10, k-indução para P5 e
  alcançabilidade composicional em 8 segmentos (o maior com 41 passos).
- **Revisão do limite de exploração (exigida pelo protocolo):** com `DT_MAX` = 200 ms, um ciclo
  completo leva 133 passos; k = 10 não basta para alcançabilidade e foi mantido apenas para P5.
- **Premissas A1–A7:** `docs/registro_verificacao.md`.

### 4.2 Resultados por propriedade (CVF)

Execução de 21/11/2026, núcleo do commit `412b609` (SHA-256 em `verification/results/versoes.txt`).

| Propriedade | Modelo (Z3) | Código (ESBMC) | Concordância |
|---|---|---|---|
| P1 | comprovado | comprovado | sim |
| P2 | comprovado | comprovado | sim |
| P3 | comprovado | comprovado | sim |
| P4 | comprovado | comprovado | sim |
| P5 | comprovado | comprovado | sim |
| P6 | comprovado | comprovado | sim |
| **CVF** | 6/6 comprovadas (100%) | 6/6 comprovadas (100%) | 6/6 |

O CVF registra a proporção das seis propriedades por categoria de resultado. Ele não representa
cobertura de código nem de todos os comportamentos (contrato, item 6.3).

### 4.3 Contraexemplos e achados

- **ESBMC-01 (corrigido):** *overflow* de `uint32` em `sat_add`, encontrado com
  `--unsigned-overflow-check`. O resultado funcional estava certo, mas dependia de *wrap-around*
  (CWE-190). O modelo Z3, com inteiros ilimitados, não pode detectar esse defeito. **Resposta a Q1:**
  as camadas concordam nas propriedades, e a camada de código acrescenta defeitos de aritmética de
  máquina.
- **Teste negativo (mutante `FSM_INJECT_BUG`, FALHA → VP no reset):** o Z3 refuta P6, e o ESBMC
  refuta step-P6, step-mem, bmc-k10 e kind-P5. Os testes unitários e a conformidade divergem.
- **Sensibilidade à versão:** com o ESBMC 8.7.0, `kind-P5` termina sem veredito (o passo indutivo
  não fecha até k = 10). Com o 8.5.0, prova em k = 2. Não houve contraexemplo, e as demais 15
  consultas coincidiram. Versão fixada em 8.5.0.

### 4.4 Ponte modelo ↔ código ↔ execução

- **Conformidade:** mesmos vetores (semente 2026) aplicados ao C e ao modelo; 8/8 estados, 16/16
  transições e 0 divergências.
- **Cenários de aceitação no simulador:** 12 cenários, 62 verificações, 0 falhas, cobrindo T1–T16.
- **Matriz de rastreabilidade:** `docs/validacao/matriz_rastreabilidade.md`.

## 5. Protocolo experimental de bancada (OE4)

Conforme o contrato, item 6.3 (comandos no `MANUAL_EXECUCAO.md`, seção 8):

- **Q2 — LREC (µs):** da entrada da ISR até o GPIO atualizado, na transição crítica T4
  (VERDE_VEICULO → ATENCAO_VEICULO). São 3 sessões com 5 ensaios de aquecimento e 10 válidos
  cada, totalizando 30 válidos.
- **Q3 — fallback:** 50 injeções válidas por tipo (1: perda do sinal 1; 2: perda do sinal 2;
  3: perda simultânea em VP; 4, extra: falha de temporização). Critério: verdes apagados e FALHA em
  até 1 s, sem retorno ao verde na janela. A recuperação via reset é medida separadamente.
- **Fixos:** placa, firmware (commit e hash), CPU de 240 MHz, FreeRTOS a 1000 Hz, período de
  10 ms, prioridades, cabo e alimentação.
- **Estatísticas:** média, mediana, desvio-padrão, IIQ, mínimo e máximo; sem p99 com n = 30.
- **Descartes:** apenas erro comprovado de instrumentação (`data/descartes.csv`), com o registro
  bruto mantido.

## 6. Resultados de bancada

> Gerar com `python analysis/analyze.py` e copiar as tabelas de `analysis/out/resumo.md`.
> **Plataforma:** [PREENCHER: ESP32 (bancada física) **ou** simulador de host — Plano B].
> Se for o Plano B, manter a frase: *"Não houve validação física; os tempos são de execução
> virtual e não são atribuíveis ao ESP32."*

### 6.1 Teste funcional (F1–F10 e B1–B5)

[PREENCHER: tabela da matriz de rastreabilidade, coluna "Bancada", com ✅ e as linhas `TR`
capturadas.]

### 6.2 Q2 — latência ISR → GPIO (µs)

| Transição | n | média | mediana | DP | IIQ | mín | máx |
|---|---|---|---|---|---|---|---|
| T4 (VV → AV) | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] |

Por sessão: [PREENCHER]. Interpretação: [PREENCHER, por exemplo dispersão entre sessões e
influência do período de controle].

### 6.3 Q3 — convergência ao estado seguro

| Tipo de falha | injeções válidas | sucessos | taxa | conv. mediana (ms) | DP | IIQ | mín | máx |
|---|---|---|---|---|---|---|---|---|
| 1 perda sinal botão 1 | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] |
| 2 perda sinal botão 2 | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] |
| 3 perda simultânea em VP | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] |
| 4 falha de temporização | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] |

Recuperação via reset: [PREENCHER]. Descartes: [PREENCHER número e motivos].

Referência do Plano B já gerada (tempo virtual): 200/200 sucessos
(`docs/validacao/evidencias/planoB_sim_falhas.md`).

## 7. Discussão: concordância e limitações (OE5)

### 7.1 Concordância entre evidências formais e experimentais

- As provas de P3, P4 e P6 dizem o que **deve** acontecer; a Q3 mostra se acontece na placa.
  [PREENCHER: os dados confirmam? Houve algum verde após falha?]
- As provas não falam de tempo físico. A Q2 mostra o custo real da arquitetura ISR → tarefa → FSM.
  [PREENCHER]

### 7.2 Limitações e ameaças à validade

1. **Sem equivalência formal completa** entre o modelo Z3 e o C. A ponte é por conformidade com
   vetores aleatórios (contrato, item 6.2).
2. **Hardware, ESP-IDF, compilador e escalonador não são verificados** (premissa A7).
3. **BMC é limitado ao horizonte**: P5 em k = 10. As garantias ilimitadas vêm de provas indutivas
   de 1 passo e de k-indução.
4. **Dependência de versão da ferramenta:** k-indução de P5 inconclusiva no ESBMC 8.7.0. Versão
   fixada em 8.5.0.
5. **Latência interna de software**, não física de ponta a ponta, salvo uso de analisador lógico.
6. **Supervisão de linha simulada por jumper**, que é uma abstração da perda real de sinal.
7. **Simulação Wokwi com núcleo próprio** e não verificado: não é usada como evidência
   (`docs/validacao/alinhamento_modelo.md`). [ATUALIZAR se a Opção A for aplicada.]
8. Protótipo didático: sem certificação nem tempo real duro.

## 8. Conclusão

[PREENCHER depois dos dados: resposta direta a Q1, Q2 e Q3; atendimento ao escopo mínimo da PTT7
(FSM explícita, alcançabilidade e ausência de deadlock verificadas) e próximos passos de
consolidação (contrato, item 10.2).]

## Apêndice A — Evidência dos objetivos específicos (contrato, item 6.1)

| Objetivo | Evidência | Situação |
|---|---|---|
| OE1 especificar a FSM | `spec/fsm_spec.md`, `spec/transition_table.csv` | pronto |
| OE2 desenvolver o firmware | `core/`, `firmware/`, `tests/` (336/0) | pronto; teste funcional em bancada pendente |
| OE3 verificação em duas camadas | `verification/`, `docs/registro_verificacao.md`, `cvf.md` | pronto (21/11/2026) |
| OE4 medir latência e fallback | `data/raw/`, `analysis/analyze.py` | pendente (bancada) |
| OE5 analisar concordância e limitações | este relatório, seção 7 | parcial |

## Apêndice B — Reprodutibilidade

- Comandos: `README.md` e `MANUAL_EXECUCAO.md`.
- Versões: `verification/results/versoes.txt` (Z3 5.4.1, ESBMC 8.5.0, gcc 13.3, ESP-IDF v5.1.4).
- Dados brutos: `data/raw/`; descartes: `data/descartes.csv`.

## Referências

BAIER, C.; KATOEN, J.-P. *Principles of Model Checking*. MIT Press, 2008.
DE MOURA, L.; BJØRNER, N. Z3: An Efficient SMT Solver. TACAS, 2008.
ESBMC. *Verification Algorithms*. Documentação oficial. Disponível em: https://esbmc.github.io/docs/theory/verification-algorithms/.
ESPRESSIF SYSTEMS. *ESP-IDF Programming Guide: High Resolution Timer (ESP Timer)*, v5.1.
ESPRESSIF SYSTEMS. *Interrupt Allocation*. Documentação do ESP-IDF.
PPCA/UFRR. *Trilhas de PTT*. Material da disciplina Arquitetura e Organização de Computadores, PTT7.
