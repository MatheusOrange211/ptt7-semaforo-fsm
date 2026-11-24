# Resultados de bancada - PTT7

Arquivos: falha1.log, falha2.log, falha3.log, falha4.log, s1_lat.log, sim_falhas.log
Plataformas consideradas nas falhas: esp32

## Q2 - Latencia interna de software (LREC = ISR -> GPIO atualizado), em us

Registros LAT: 23 | aquecimento excluido: 6 | descartados (data/descartes.csv): 0 | validos: 17

| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| FALHA T0 | 1 | 14.0 | 14.0 | 0.0 | 0.0 (14.0-14.0) | 14.0 | 14.0 |
| PEDIDO_PEDESTRE T0 | 7 | 13.9 | 13.0 | 1.9 | 0.5 (13.0-13.5) | 13.0 | 18.0 |
| RECOVERY T0 | 6 | 13.8 | 14.0 | 0.4 | 0.0 (14.0-14.0) | 13.0 | 14.0 |
| VERDE_VEICULO T3 | 3 | 13.7 | 14.0 | 0.6 | 0.5 (13.5-14.0) | 13.0 | 14.0 |

Latencia ate a decisao da FSM (antes de escrever os GPIOs), em us:

| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| FALHA T0 | 1 | 12.0 | 12.0 | 0.0 | 0.0 (12.0-12.0) | 12.0 | 12.0 |
| PEDIDO_PEDESTRE T0 | 7 | 12.1 | 12.0 | 1.8 | 1.0 (11.0-12.0) | 11.0 | 16.0 |
| RECOVERY T0 | 6 | 12.0 | 12.0 | 0.0 | 0.0 (12.0-12.0) | 12.0 | 12.0 |
| VERDE_VEICULO T3 | 3 | 11.7 | 12.0 | 0.6 | 0.5 (11.5-12.0) | 11.0 | 12.0 |

Por sessao (transicao critica VERDE_VEICULO T4 -> ATENCAO_VEICULO):

| Sessao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|

Protocolo (>= 30 medicoes validas em >= 3 sessoes para T4): NAO atendido (0 validas, 0 sessoes).
Percentis de cauda (p99) nao sao reportados: amostra insuficiente para evidencia robusta.
Latencia fim a fim (fisica) so pode ser afirmada com analisador logico.

## Q3 - Convergencia ao estado seguro (FALHA, amarelo piscante)

Criterio: verdes apagados e FALHA alcancado em ate 1 s apos a injecao, sem retorno ao verde na janela.

| Tipo de falha | injecoes validas | sucessos | taxa de sucesso | conv. media (ms) | mediana | DP | IIQ | min | max |
|---|---|---|---|---|---|---|---|---|---|
| perda sinal botao 1 | 50 | 50 | 100.0% | 4.67 | 4.43 | 2.86 | 5.00 | 0.42 | 9.43 |
| perda sinal botao 2 | 50 | 50 | 100.0% | 5.67 | 5.43 | 2.63 | 3.75 | 0.42 | 9.43 |
| perda simultanea em VP | 50 | 50 | 100.0% | 4.68 | 4.42 | 2.78 | 5.00 | 0.42 | 9.42 |
| falha de temporizacao | 50 | 50 | 100.0% | 305.22 | 305.43 | 3.13 | 5.50 | 300.42 | 309.43 |

Protocolo (>= 50 injecoes validas por tipo 1-3): ATENDIDO.

## Recuperacao via reset (avaliada separadamente)

| Tipo de falha anterior | ensaios | sucesso | ms ate RECOVERY (mediana) | ms ate VERDE (mediana) |
|---|---|---|---|---|
| perda sinal botao 1 | 50 | 50 | 5.0 | 3005.0 |
| perda sinal botao 2 | 50 | 50 | 5.0 | 3005.0 |
| perda simultanea em VP | 50 | 50 | 5.0 | 3005.0 |
| falha de temporizacao | 50 | 50 | 5.0 | 3005.0 |

## Descartes

Registros #DESCARTE emitidos pelo firmware: 4
- s1_lat.log:5 #DESCARTE,lat,1,0,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)
- s1_lat.log:6 #DESCARTE,lat,1,1,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)
- s1_lat.log:7 #DESCARTE,lat,1,2,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)
- s1_lat.log:8 #DESCARTE,lat,1,3,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)

matplotlib nao instalado: graficos nao gerados (pip install matplotlib).
