# Resultados de bancada - PTT7

Arquivos: falha1.log, falha2.log, falha3.log, falha4.log, reteste_s1_lat.log, reteste_s2_lat.log, reteste_s3_lat.log, reteste_s4_lat.log, reteste_s5_lat.log, reteste_s6_lat.log, s1_lat.log, sim_falhas.log
Plataformas consideradas nas falhas: esp32

## Q2 - Latencia interna de software (LREC = ISR -> GPIO atualizado), em us

Registros LAT: 59 | fora de experimento 'lat' (incidentais, nao contam): 17 | aquecimento excluido: 12 | descartados (data/descartes.csv): 0 | validos: 30

| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| VERDE_VEICULO T4 | 30 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |

Latencia ate a decisao da FSM (antes de escrever os GPIOs), em us:

| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| VERDE_VEICULO T4 | 30 | 18.0 | 18.0 | 0.0 | 0.0 (18.0-18.0) | 18.0 | 18.0 |

Por sessao (transicao critica VERDE_VEICULO T4 -> ATENCAO_VEICULO):

| Sessao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| 1 | 30 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |

Por janela de captura (arquivo) - descritivo, nao substitui o rotulo de sessao:

| Arquivo | uptime no inicio (s) | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|---|
| reteste_s1_lat.log | 741.3 | 5 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |
| reteste_s2_lat.log | 926.7 | 5 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |
| reteste_s3_lat.log | 1199.3 | 5 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |
| reteste_s4_lat.log | 1973.1 | 5 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |
| reteste_s5_lat.log | 2380.6 | 5 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |
| reteste_s6_lat.log | 2567.6 | 5 | 20.0 | 20.0 | 0.0 | 0.0 (20.0-20.0) | 20.0 | 20.0 |

Protocolo (>= 30 medicoes validas em >= 3 sessoes para T4): NAO atendido (30 validas, 1 sessoes).
Situacao: PARCIALMENTE atendido - volume atingido (30 validas em 6 janelas de captura), mas as janelas nao foram separadas como sessoes (1 rotulo de sessao).
Percentis de cauda (p99) nao sao reportados: amostra insuficiente para evidencia robusta.
Latencia fim a fim (fisica) so pode ser afirmada com analisador logico.

### Q2 - Evidencia exploratoria (NAO atende ao protocolo)

Todos os registros LAT do ESP32, incluindo aquecimento e acionamentos incidentais de botao
durante os ensaios de falha. Mesmo trecho cronometrado (esp_timer_get_time); mistura condicoes
diferentes (com e sem estimulo controlado): serve apenas como ordem de grandeza.

| Grupo | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| todos (ISR -> GPIO) | 59 | 17.6 | 20.0 | 3.1 | 6.0 (14.0-20.0) | 13.0 | 20.0 |
| dentro do experimento lat (aquecimento) | 42 | 19.1 | 20.0 | 2.2 | 0.0 (20.0-20.0) | 13.0 | 20.0 |
| incidentais (fora do experimento lat) | 17 | 13.8 | 14.0 | 1.2 | 1.0 (13.0-14.0) | 13.0 | 18.0 |
| somente T4 (inclui aquecimento) | 38 | 19.6 | 20.0 | 1.5 | 0.0 (20.0-20.0) | 13.0 | 20.0 |

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

Graficos: latencia_boxplot.png, convergencia_boxplot.png
