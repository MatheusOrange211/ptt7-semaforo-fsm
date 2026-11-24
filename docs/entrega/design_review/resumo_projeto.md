# PTT7 — Resumo do projeto para o design review (24/11/2026)

**Título:** Controle de semáforo com travessia de pedestres baseado em máquina de estados no ESP32
com verificação em duas camadas e avaliação da latência de resposta.
**Equipe:** Pedro Vinicius da Silva Ribeiro e Matheus Naranjo Corrêa.
**Orientador:** Prof. Herbert Rocha · **Trilha:** PTT7 — Controle baseado em FSM · **Entrega:** 26/11/2026.

## Problema

Um semáforo é um sistema em que um erro de lógica pode ser perigoso: por exemplo, verde para os
carros e para os pedestres ao mesmo tempo. Testes comuns só verificam alguns caminhos. A pergunta
do projeto (contrato, item 2.2) é se a verificação formal em duas camadas, do modelo e do código,
demonstra que as propriedades de segurança se mantêm diante de botões apertados a qualquer
momento e de falhas de temporização, e qual é o custo disso em latência e robustez na bancada.

## O que o Catálogo PTT7 exige e como atendemos

| Exigência do Catálogo | Como o projeto atende |
|---|---|
| Firmware com FSM explícita e documentada | `core/fsm_core.c` (8 estados, 16 transições, tabela em `spec/`) + firmware ESP-IDF em `firmware/` |
| Operação com requisito de confiabilidade | intertravamento veículo × pedestre e recuperação após falha |
| Modelo verificado: alcançabilidade e ausência de deadlock | P1 e P2 provadas no Z3 (modelo) e no ESBMC (código) |
| Artefatos: firmware, modelo formal, saída da verificação, evidência de bancada, relatório | os quatro primeiros existem no repositório; bancada e relatório em andamento |
| Plano B: simulação com atuador emulado | simulador de host (`sim/`) com o mesmo núcleo e cenários de aceitação |

## Arquitetura (em uma frase)

Um único arquivo de lógica, `fsm_core.c`, **sem nenhum acesso a hardware** e com o tempo passado
como entrada, é compilado no firmware do ESP32, nos testes, no simulador e analisado pelo ESBMC.
O hardware (GPIO, interrupções dos botões e tarefas FreeRTOS) fica num invólucro separado.

```
spec/transition_table.csv ──► core/fsm_core.c ──► firmware ESP32 (GPIO, ISR, FreeRTOS)
                                   │       │
                     ESBMC (código)│       │testes + simulador (Plano B)
                                   │
          modelo Z3 ◄── conformidade (mesmos vetores de entrada, 0 divergências)
```

## A máquina de estados

8 estados: INIT, VERDE_VEICULO, PEDIDO_PEDESTRE, ATENCAO_VEICULO, VERMELHO_VEICULO_VERDE_PEDESTRE,
ATENCAO_PEDESTRE, FALHA (amarelo piscante) e RECOVERY. São 16 transições (T1–T16). Falha = perda
de sinal do botão 1 ou 2, ou passo atrasado mais que 200 ms; ela tem prioridade em todos os estados.
Tempos: verde mínimo de 8 s, amarelo de 3 s, verde de pedestre de 6 s.

## Propriedades e resultados (reexecução de 21/11/2026)

| Propriedade | Modelo (Z3) | Código (ESBMC 8.5.0) |
|---|---|---|
| P1 alcançabilidade dos 8 estados | comprovado | comprovado |
| P2 ausência de deadlock | comprovado | comprovado |
| P3 exclusão mútua dos verdes (+ verde mínimo) | comprovado | comprovado |
| P4 violação das entradas → FALHA | comprovado | comprovado |
| P5 convergência do fallback | comprovado | comprovado |
| P6 FALHA não volta direto a VP (passa por RECOVERY) | comprovado | comprovado |

- **A verificação detecta erros:** um mutante que pula RECOVERY é refutado nas duas camadas.
- **A segunda camada se justifica:** o ESBMC achou um *overflow* aritmético que o Z3 não poderia
  achar (achado ESBMC-01, corrigido).
- **Validação:** as 16 transições têm evidência em teste unitário, em conformidade com o modelo
  e em cenário de aceitação (`docs/validacao/matriz_rastreabilidade.md`).

## Status e riscos

| Item | Situação |
|---|---|
| Especificação, núcleo, modelo e verificação | prontos e reconfirmados em 21/11 |
| Firmware ESP-IDF com instrumentação de latência e injeção de falhas | pronto |
| Bancada física (Q2 latência, Q3 fallback) | montagem em ajuste (problema de GND/pinagem) |
| Simulação Wokwi | usa um núcleo próprio, divergente do verificado; alinhamento proposto |
| Relatório e vídeo | estrutura pronta, aguardando dados de bancada |

> **Atualização 24/11/2026:** a bancada foi executada com a nova pinagem (supervisão por software, `PTT7_SUPERVISAO_HW=0`): Q3 completa, Q2 não atendida (exploratória). Ver `docs/validacao/evidencias/resultados_bancada_2026-11-24.md`.

**Plano B (contrato, item 9):** se a placa não ficar pronta, os experimentos rodam no simulador
de host, declarando a ausência de validação física e sem atribuir tempos ao ESP32.

## Fora do escopo (contrato, item 6.2)

Certificação normativa, prova de tempo real duro, integração industrial, múltiplos nós, rede,
verificação do ESP-IDF, do compilador ou do hardware, e equivalência formal completa entre modelo
e firmware.
