# Semáforo com FSM no ESP32 — PTT7

Protótipo de controle de semáforo para veículos e pedestres, desenvolvido para o projeto PTT7 de Arquitetura e Organização de Computadores.

## Simulação

[Executar no Wokwi](COLE_AQUI_O_LINK_DO_PROJETO_WOKWI)

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

## Próximas etapas

- Consolidar a especificação e a tabela de transições.
- Implementar a verificação com Z3 e ESBMC.
- Integrar o firmware com ESP-IDF e notificações de tarefa FreeRTOS.
- Ampliar a instrumentação e executar o protocolo de repetições.
- Realizar os experimentos de bancada e finalizar o relatório técnico.