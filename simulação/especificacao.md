# Semáforo com FSM no ESP32 — PTT7

Protótipo de controle de semáforo para veículos e pedestres, desenvolvido para o projeto PTT7 de Arquitetura e Organização de Computadores.

> **Atenção:** esta simulação usa uma FSM própria, **diferente do núcleo verificado** (`core/`), e não é
> evidência das propriedades P1–P6 nem dos experimentos Q2/Q3. Leia o [`README.md`](README.md) desta
> pasta. Este documento descreve a etapa inicial do projeto e foi mantido como registro.

## Simulação

Não há link público do projeto no Wokwi; para executar, siga a seção "Como executar".

A versão atual executa uma bateria automática de sete cenários. Os pedidos, resets e indicadores de falha são gerados pelo programa.

## Arquivos

- `sketch.ino`: integração com os LEDs e execução dos testes automáticos.
- `diagram.json`: componentes e conexões do circuito no Wokwi.
- `fsm_core.h`: estados, estruturas, temporizações e interface do núcleo.
- `fsm_core.c`: lógica da máquina de estados, sem chamadas de hardware.
- `resultados_simulacao.txt`: registro da rodada de testes realizada.

## Como executar

1. Abra um novo projeto ESP32 no Wokwi.
2. Copie os arquivos para as abas correspondentes, preservando seus nomes.
3. Inicie a simulação.
4. Aguarde o resumo dos sete cenários no monitor serial.

Não são necessárias bibliotecas externas.

## Cenários automatizados

1. Pedido antecipado e respeito ao tempo mínimo de verde.
2. Pedido tardio e ciclo normal.
3. Pedidos simultâneos representados por uma entrada lógica agrupada.
4. Falha artificial do canal 1 durante a travessia.
5. Falha artificial do canal 2 durante a travessia.
6. Falhas artificiais simultâneas durante a travessia.
7. Atraso de execução de 400 ms.

Os testes de falha também verificam bloqueio, pedido durante falha, retirada da falha sem reset e recuperação. Nos cenários de falha de canal, é testado reset com falha ativa.

## Resultado preliminar

A rodada registrada terminou com sete cenários aprovados.

## Limitações

Os testes automáticos usam entradas artificiais e não validam as interrupções ou o debounce dos botões físicos. As falhas de canal representam indicadores de diagnóstico, sem detectar perda elétrica real de sinal.

Os resultados são evidências funcionais de simulação. Não constituem prova formal, medição de latência física do ESP32 ou certificação de segurança.

## Situação das etapas seguintes

As etapas previstas aqui (tabela de transições, verificação com Z3 e ESBMC, firmware ESP-IDF com
notificações do FreeRTOS, instrumentação e bancada) foram realizadas no projeto principal, sobre o
núcleo verificado em `core/`, e não nesta simulação. Ver o `README.md` na raiz do repositório.