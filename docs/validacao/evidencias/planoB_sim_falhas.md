# Plano B — injeção de falhas no simulador de host (tempo virtual)

> **Não é evidência de bancada.** Tempos em passo virtual de 10 ms, **não atribuíveis ao ESP32**
> (contrato, item 9). Serve apenas como reserva caso a placa não fique pronta, e como conferência
> lógica de P4/P5 em execução contínua.

Gerado em 21/11/2026 com o mesmo `core/fsm_core.c` verificado:

```
./build/ptt7_sim --falhas 50 --semente 2026 > data/raw/sim_falhas.log
python3 analysis/analyze.py data/raw/sim_falhas.log --plataforma sim
```

Leitura: a convergência de 0 a 10 ms nos tipos 1–3 é o próprio passo de 10 ms do simulador (a
falha é vista no passo seguinte, como diz P4). No tipo 4, os ~295 ms incluem o atraso injetado de
300 ms, que é o que define a falha de temporização.

## Q3 (simulador) - Convergencia ao estado seguro (FALHA, amarelo piscante)

Criterio: verdes apagados e FALHA alcancado em ate 1 s apos a injecao, sem retorno ao verde na janela.

| Tipo de falha | injecoes validas | sucessos | taxa de sucesso | conv. media (ms) | mediana | DP | IIQ | min | max |
|---|---|---|---|---|---|---|---|---|---|
| perda sinal botao 1 | 50 | 50 | 100.0% | 4.92 | 4.80 | 2.98 | 4.57 | 0.15 | 10.00 |
| perda sinal botao 2 | 50 | 50 | 100.0% | 5.24 | 5.29 | 3.19 | 5.65 | 0.04 | 9.92 |
| perda simultanea em VP | 50 | 50 | 100.0% | 4.53 | 4.54 | 3.04 | 5.19 | 0.05 | 9.85 |
| falha de temporizacao | 50 | 50 | 100.0% | 294.92 | 294.65 | 3.00 | 5.30 | 290.21 | 299.77 |

Protocolo (>= 50 injecoes validas por tipo 1-3): ATENDIDO.

