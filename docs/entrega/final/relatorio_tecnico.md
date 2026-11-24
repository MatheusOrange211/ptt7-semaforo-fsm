# Relatório técnico conclusivo — PTT7

**Controle de semáforo com travessia de pedestres baseado em máquina de estados no ESP32 com
verificação em duas camadas e avaliação da latência de resposta**

Pedro Vinicius da Silva Ribeiro · Matheus Naranjo Corrêa — Orientador: Prof. Herbert Rocha
Arquitetura e Organização de Computadores · PPCA/UFRR · 2026.2 · Entrega: 26/11/2026

> **Estado deste documento (24/11/2026):** reflete os resultados de bancada obtidos em 24/11/2026
> no ESP32 físico (Q3 com protocolo atendido; Q2 respondida de forma exploratória, com o protocolo
> **não** atendido). Os números vêm de `python analysis/analyze.py` e estão consolidados em
> `docs/validacao/evidencias/resultados_bancada_2026-11-24.md`. A estrutura segue o contrato:
> item 10.1 (resultados por propriedade, medições, contraexemplos, limitações e evidências dos
> testes) e item 6.1 (evidência de cada objetivo específico).

---

## Resumo

Este relatório apresenta um controlador de semáforo com travessia de pedestres modelado como
máquina de estados finitos (FSM) de 8 estados e 16 transições, implementado no ESP32 com o núcleo
lógico isolado em C. Seis propriedades de segurança e vivacidade (P1–P6) foram verificadas em duas
camadas: o modelo abstrato com Z3 e o próprio código C com ESBMC. Todas foram comprovadas nas duas
camadas, com poder de detecção demonstrado por um mutante. A camada de código encontrou um defeito
de *overflow* invisível ao modelo. Na bancada física (ESP32, 24/11/2026), o fallback por injeção
de falhas em software convergiu ao estado seguro em 200/200 injeções (50 por tipo), sem nenhum
retorno ao verde. O protocolo de latência (Q2, 30 medições válidas de T4 em 3 sessões) **não foi
atendido** (0 medições válidas); como evidência apenas exploratória, 23 registros reais de LREC
(ISR → GPIO atualizado) tiveram mediana de 14 µs. Não há medição de latência fim a fim física.

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
- **Bancada:** ESP32 DevKit V1, 5 LEDs com resistores e 3 botões (`docs/bancada.md`). Nos
  ensaios de 24/11/2026 a supervisão de linha por hardware ficou desligada
  (`PTT7_SUPERVISAO_HW = 0`) e a perda de sinal foi injetada por software (ver seção 6).
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
  (VERDE_VEICULO → ATENCAO_VEICULO). Protocolo previsto: 3 sessões com 5 ensaios de aquecimento
  e 10 válidos cada, totalizando 30 válidos (não atendido; ver seção 6.2).
- **Q3 — fallback:** 50 injeções válidas por tipo (1: perda do sinal 1; 2: perda do sinal 2;
  3: perda simultânea em VP; 4, extra: falha de temporização). Critério: verdes apagados e FALHA em
  até 1 s, sem retorno ao verde na janela. A recuperação via reset é medida separadamente.
- **Fixos:** placa, firmware (commit e hash), CPU de 240 MHz, FreeRTOS a 1000 Hz, período de
  10 ms, prioridades, cabo e alimentação.
- **Estatísticas:** média, mediana, desvio-padrão, IIQ, mínimo e máximo; sem p99 com n = 30.
- **Descartes:** apenas erro comprovado de instrumentação (`data/descartes.csv`), com o registro
  bruto mantido.

## 6. Resultados de bancada

> Tabelas geradas por `python analysis/analyze.py` (`analysis/out/resumo.md`).
> **Plataforma:** ESP32 em bancada física, 24/11/2026 (ESP-IDF 5.1.4, CPU 240 MHz, FreeRTOS a
> 1000 Hz, núcleo em IRAM, console a 115200). Pinagem adaptada em `firmware/main/app_config.h`:
> LED do carro VM/AM/VD = GPIO 25/26/27; LED do pedestre VM/VD = 18/19; BTN_PED1 = 33,
> BTN_PED2 = 32, BTN_RESET = 21; STIM1/STIM2 = 16/17 (*open-drain*). SIG1/SIG2 (GPIO 22/23) estão
> configurados, mas são **ignorados** porque `PTT7_SUPERVISAO_HW = 0`.
>
> **Proveniência:** firmware compilado a partir do commit `1ff400d` (declarado pela equipe) com
> alteração local apenas em `firmware/main/app_config.h` (pinagem). `core/fsm_core.c`,
> `fsm_core.h`, `fsm_config.h` e `spec/transition_table.csv` têm SHA-256 idênticos aos de
> `verification/results/versoes.txt`, ou seja, é o mesmo código verificado por ESBMC/Z3. Os logs
> não contêm o banner de boot (captura iniciada com a placa já ligada): a build é **declarada, não
> comprovada pelo log**. A placa não reiniciou entre as capturas de falha (*uptime* contínuo de
> cerca de 45 min).
>
> **Injeção em software:** com `HW = 0`, a perda de sinal é injetada por flags de software
> (`s_inj_loss1/2`, comandos `falha`/`perda`/`restaura`) e a falha de temporização por atraso
> injetado (`s_inj_stall_ms`). O caminho físico dos GPIO 22/23 **não foi exercitado**. Isso é
> coerente com a Q3 do projeto ("falhas controladas em tempo de execução"), mas é uma limitação
> (seção 7.2). O Plano B (simulador de host) permanece apenas como referência/contingência e
> **não** foi usado como evidência da Q3.

### 6.1 Teste funcional (F1–F10 e B1–B5)

O teste funcional passo a passo (F1–F10) e os itens B1–B5 **não foram executados como roteiro
formal** nesta bancada. Os itens B1–B5 foram escritos para os jumpers dos GPIO 22/23, que ficam
ignorados com `HW = 0`. O equivalente por injeção de software existe apenas onde os ensaios de
falha cobriram o estado: Q3 tipos 1–3 (`perda`, `restaura`) e tipo 4 (atraso injetado). O estado
em que cada falha foi injetada está na seção 6.3. A coluna "Bancada" de
`docs/validacao/matriz_rastreabilidade.md` foi atualizada nesse sentido.

### 6.2 Q2 — latência ISR → GPIO (µs): respondida de forma exploratória

**O protocolo da Q2 não foi atendido: 0 medições válidas de T4 e 1 sessão (as sessões 2 e 3 não
foram realizadas), contra o mínimo de 30 medições válidas em 3 sessões.** Por isso a Q2 é
respondida apenas de forma **exploratória**; não há estatística de T4 conforme o protocolo. A
tabela por transição do protocolo ficou vazia (n = 0) e não é reproduzida aqui.

Causas, relatadas sem atenuação:

1. **O estímulo não chegou ao botão.** O firmware registrou 4 vezes `#DESCARTE ... estimulo nao
   detectado (verifique jumper STIM1->BTN_PED1)` (`s1_lat.log`, seq 0–3). Causa provável: após a
   troca de pinos, o jumper precisava ir de GPIO16 → GPIO33, mas a documentação ainda indicava
   16 → 18. Não confirmado pela equipe.
2. **As duas execuções de `lat 10 5` abortaram** (`#AVISO ensaio N: sistema nao voltou a
   VERDE_VEICULO com verde minimo; abortando`): a primeira no ensaio 5 e a segunda no ensaio 1,
   ainda no aquecimento.
3. **A segunda captura (11:59) do `s1_lat.log` está corrompida:** cerca de 99,6% do arquivo
   (1,4 MB, 54 mil linhas) são fragmentos repetidos (`=10 aquecimento=5 botao=1`). Causa provável:
   porta serial aberta por dois programas ao mesmo tempo (por exemplo, `idf.py monitor` e
   `capture_serial.py`). O log bruto foi mantido sem alteração, como manda o protocolo.
4. **Não houve nova rodada de bancada** antes do prazo de entrega.

**Evidência exploratória (não substitui o protocolo).** Foram recuperados 23 registros LAT reais
do ESP32, medidos no mesmo trecho cronometrado (ISR → FSM → GPIO, `esp_timer_get_time`):

| Grupo | n | média (µs) | mediana (µs) | DP (µs) | mín (µs) | máx (µs) |
|---|---|---|---|---|---|---|
| todos (ISR → GPIO atualizado = LREC) | 23 | 13,8 | 14,0 | 1,0 | 13 | 18 |
| dentro do experimento `lat` (todos aquecimento) | 6 | 13,7 | 14,0 | 0,5 | 13 | 14 |
| incidentais (toques de botão durante ensaios de falha: 16 em `falha1`, 1 em `falha2`) | 17 | 13,8 | 14,0 | 1,2 | 13 | 18 |
| somente T4 (ambas aquecimento) | 2 | 13,5 | 13,5 | 0,7 | 13 | 14 |

A latência até a decisão da FSM (antes de escrever os GPIOs) teve mediana de 12 µs (11–16 µs,
n = 23). **Interpretação, com cautela:** a ordem de grandeza de cerca de 14 µs é aproximadamente
700 vezes menor que o período de controle de 10 ms, o que indica que o custo ISR → tarefa → FSM →
GPIO é pequeno frente ao período. Isso não é uma medição conforme o protocolo: não há estímulo
controlado, não há três sessões, o n é pequeno e a origem dos 17 toques incidentais (manual ou
ruído) não foi confirmada. Não é reportado p99. A latência fim a fim física **não foi medida**
(sem analisador lógico). Possível risco de hardware, não confirmado: se o jumper ficou em
GPIO16 → GPIO18, esse pino hoje é saída (LED vermelho do pedestre), com contenção breve (cerca de
20 ms) a cada pulso.

**Evidência complementar: latência lógica e arquitetura (não mede tempo do ESP32).** Para separar
o atraso que a *lógica* poderia introduzir do custo de execução, o próprio `core/fsm_core.c` foi
executado no computador (`tests/test_t4_latencia_logica.c`). O pedido de pedestre foi posicionado
em todos os instantes de 0 a 20 s após o verde mínimo, em passos de 10 ms, para os dois botões
(4.002 casos). Em **4.002/4.002 casos a T4 ocorreu no próprio passo da FSM que recebeu o pedido**,
ou seja, a lógica não acrescenta nenhum passo de espera. No firmware, a ISR do botão acorda a
tarefa de controle diretamente (`vTaskNotifyGiveFromISR` + `portYIELD_FROM_ISR`), sem aguardar o
próximo período de 10 ms. Com isso, a Q2 se apoia em três evidências de naturezas diferentes:
lógica (simulação do núcleo: 0 passos extras), arquitetura (notificação direta da ISR) e física
exploratória (LREC ≈ 14 µs, n = 23). Conforme o Plano B do contrato, **nenhum tempo obtido no
computador é atribuído ao ESP32**, e o protocolo da Q2 continua **não atendido**.

**Correção no `analyze.py` feita após observar os dados** (documentada): um LAT só conta para o
protocolo se estiver dentro de um bloco `#INICIO lat ... #FIM lat`; os incidentais aparecem apenas
na tabela exploratória. Também foi corrigida a leitura de `data/descartes.csv` (linhas de
comentário faziam o script travar). A correção não altera o resultado da Q3.

### 6.3 Q3 — convergência ao estado seguro

Falhas injetadas por software no ESP32 físico (24/11/2026). Critério: verdes apagados e FALHA em
até 1 s, sem retorno ao verde na janela; mínimo de 50 injeções válidas por tipo. **Protocolo
atendido.** Logs: `data/raw/falha1.log` a `falha4.log` (sessão 1, seq 0–49 sem lacunas, sem
*panic*, *reset* ou *brownout*).

| Tipo de falha | injeções válidas | sucessos | taxa | conv. média (ms) | mediana (ms) | DP | IIQ | mín | máx |
|---|---|---|---|---|---|---|---|---|---|
| 1 perda sinal botão 1 | 50 | 50 | 100% | 4,67 | 4,43 | 2,86 | 5,00 | 0,42 | 9,43 |
| 2 perda sinal botão 2 | 50 | 50 | 100% | 5,67 | 5,43 | 2,63 | 3,75 | 0,42 | 9,43 |
| 3 perda simultânea em VP | 50 | 50 | 100% | 4,68 | 4,42 | 2,78 | 5,00 | 0,42 | 9,42 |
| 4 falha de temporização | 50 | 50 | 100% | 305,22 | 305,43 | 3,13 | 5,50 | 300,42 | 309,43 |

`verde_apos_falha` = 0 nas 200 injeções; 0 descartes de falha. Nos tipos 1–3, a convergência
(0,42–9,43 ms) é limitada pelo período de controle de 10 ms. No tipo 4, o atraso injetado foi
`FSM_DT_MAX_MS` + 100 = 300 ms, e a convergência de cerca de 305 ms corresponde à detecção após
esse atraso (resultado esperado, não uma latência de reação). Estado do sistema na injeção:
falha 1 em VERDE_VEICULO 44 / PEDIDO_PEDESTRE 6; falha 2 em VERDE_VEICULO 49 / PEDIDO_PEDESTRE 1;
falha 3 em VERMELHO_VEICULO_VERDE_PEDESTRE 50; falha 4 em VERDE_VEICULO 50.

**Recuperação via reset** (avaliada separadamente):

| Tipo de falha anterior | ensaios | sucesso | até RECOVERY (mediana, ms) | até VERDE_VEICULO (mediana, ms) |
|---|---|---|---|---|
| 1 perda sinal botão 1 | 50 | 50 | 5,0 | 3005,0 |
| 2 perda sinal botão 2 | 50 | 50 | 5,0 | 3005,0 |
| 3 perda simultânea em VP | 50 | 50 | 5,0 | 3005,0 |
| 4 falha de temporização | 50 | 50 | 5,0 | 3005,0 |

A resolução é de 5 ms (laço de espera); RECOVERY dura 3 s por projeto. **Descartes:** 0 descartes
de falha. Os 4 registros `#DESCARTE` do firmware pertencem à Q2 (causa 1 da seção 6.2).

Referência do Plano B (tempo virtual, simulador de host; contingência, **não** usada como
evidência da Q3): 200/200 sucessos (`docs/validacao/evidencias/planoB_sim_falhas.md`).

## 7. Discussão: concordância e limitações (OE5)

### 7.1 Concordância entre evidências formais e experimentais

- As provas de P3, P4 e P6 dizem o que **deve** acontecer; a Q3 mostra se acontece na placa.
  Na bancada, os dados são coerentes com isso: em 200/200 injeções houve convergência a FALHA em
  até 1 s e **nenhum** retorno ao verde na janela. A concordância vale para falhas injetadas por
  software; não prova a cadeia física de supervisão.
- As provas não falam de tempo físico. A Q2 mostra o custo real da arquitetura ISR → tarefa → FSM.
  A evidência exploratória (mediana de cerca de 14 µs, ISR → GPIO) sugere custo pequeno frente
  ao período de 10 ms, mas, como o protocolo da Q2 não foi atendido, isso é ordem de grandeza e
  não uma medição conclusiva.

### 7.2 Limitações e ameaças à validade

1. **Sem equivalência formal completa** entre o modelo Z3 e o C. A ponte é por conformidade com
   vetores aleatórios (contrato, item 6.2).
2. **Hardware, ESP-IDF, compilador e escalonador não são verificados** (premissa A7).
3. **BMC é limitado ao horizonte**: P5 em k = 10. As garantias ilimitadas vêm de provas indutivas
   de 1 passo e de k-indução.
4. **Dependência de versão da ferramenta:** k-indução de P5 inconclusiva no ESBMC 8.7.0. Versão
   fixada em 8.5.0.
5. **Latência interna de software**, não física de ponta a ponta, salvo uso de analisador lógico.
6. **Injeção de falhas por software** (`PTT7_SUPERVISAO_HW = 0`): a perda de sinal e o atraso
   foram injetados por flags, e o caminho físico dos GPIO 22/23 não foi exercitado. Isso
   atende à Q3 ("falhas controladas em tempo de execução"), mas não valida a fiação de supervisão.
7. **Simulação Wokwi com núcleo próprio** e não verificado: não é usada como evidência
   (`docs/validacao/alinhamento_modelo.md`). [ATUALIZAR se a Opção A for aplicada.]
8. Protótipo didático: sem certificação nem tempo real duro. Os resultados não provam o
   cumprimento de prazos de tempo real.
9. **Q2 não atendeu ao protocolo** (0 medições válidas, 1 sessão); só há evidência exploratória
   (n = 23, sem p99), parte dela de toques incidentais de origem não confirmada.
10. **Captura corrompida:** a segunda captura do `s1_lat.log` tem cerca de 99,6% de linhas
    repetidas (provável acesso simultâneo à porta serial); o log bruto foi mantido.
11. **Build declarada, não registrada no log:** os logs não têm o banner de boot, então o commit
    `1ff400d` é declarado pela equipe; a identidade do núcleo é sustentada pelos SHA-256.
12. **Sem analisador lógico:** não há latência fim a fim física, nem p99.
13. **Correção no `analyze.py` após ver os dados** (filtro por bloco `#INICIO lat ... #FIM lat`):
    documentada na seção 6.2; não altera a Q3, mas é uma decisão tomada com os dados à vista.

## 8. Conclusão

- **Q1 (modelo e código concordam?):** sim. As seis propriedades (P1–P6) foram comprovadas nas
  duas camadas (CVF 6/6), com poder de detecção demonstrado por mutante; a camada de código
  acrescentou o achado ESBMC-01 (*overflow*).
- **Q2 (latência até a transição para o amarelo):** respondida **de forma exploratória**. O
  protocolo não foi atendido (0 medições válidas de T4, 1 sessão). A evidência exploratória
  (n = 23) indica LREC de cerca de 14 µs (mediana), ISR → GPIO atualizado, sem medição fim a fim
  física e sem p99. Não é uma resposta conclusiva.
- **Q3 (frequência e tempo do fallback):** respondida, com protocolo atendido. Em 200/200
  injeções por software (50 por tipo) o sistema atingiu FALHA em até 1 s, sem retorno ao verde:
  4,4 a 5,4 ms de mediana nos tipos 1–3 e cerca de 305 ms no tipo 4 (detecção após o atraso
  injetado de 300 ms). Recuperação via reset: 200/200.

O escopo mínimo da PTT7 (FSM explícita, alcançabilidade e ausência de deadlock verificadas) foi
atendido. A pendência principal é refazer a Q2 conforme o protocolo; trabalho futuro está na
seção 9.

## 9. Trabalho futuro: reteste da Q2

Com os jumpers corrigidos para GPIO16 → GPIO33 e GPIO17 → GPIO32 e o `idf.py monitor` fechado
(para não disputar a porta serial), repetir:

```bash
for s in 1 2 3; do
  python tools/capture_serial.py --porta <PORTA> --saida data/raw/s${s}_lat.log \
    --cmd "sessao ${s}" --cmd "lat 10 5"
done
python analysis/analyze.py
```

Também seria desejável registrar o banner de boot na captura (para comprovar a build) e usar um
analisador lógico para a latência fim a fim e para exercitar a supervisão física (`HW = 1`).

## Apêndice A — Evidência dos objetivos específicos (contrato, item 6.1)

| Objetivo | Evidência | Situação |
|---|---|---|
| OE1 especificar a FSM | `spec/fsm_spec.md`, `spec/transition_table.csv` | pronto |
| OE2 desenvolver o firmware | `core/`, `firmware/`, `tests/` (336/0) | pronto; firmware executado no ESP32 físico (24/11/2026), teste funcional via ensaios de falha |
| OE3 verificação em duas camadas | `verification/`, `docs/registro_verificacao.md`, `cvf.md` | pronto (21/11/2026) |
| OE4 medir latência e fallback | `data/raw/`, `analysis/analyze.py`, `docs/validacao/evidencias/resultados_bancada_2026-11-24.md` | **parcial**: Q3 completo (200/200); Q2 apenas exploratório |
| OE5 analisar concordância e limitações | este relatório, seção 7 | pronto (concordância e limites discutidos) |

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
