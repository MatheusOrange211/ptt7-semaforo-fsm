# Resultados de bancada física (ESP32) — 24/11/2026

> **Origem deste arquivo.** As tabelas abaixo são a saída de `python analysis/analyze.py` sobre os logs
> brutos de `data/raw/` (`falha1.log`–`falha4.log`, `s1_lat.log`). Como `analysis/out/` não é versionado
> (`.gitignore`), esta cópia preserva o resultado no repositório. Os logs brutos não foram editados.

## Configuração do ensaio

| Item | Valor |
|---|---|
| Plataforma | ESP32 físico (ESP32 DevKit V1 / WROOM-32), ESP-IDF 5.1.4, CPU 240 MHz, FreeRTOS 1000 Hz, núcleo em IRAM |
| Firmware | commit `1ff400d` (declarado pela equipe) + pinagem adaptada em `firmware/main/app_config.h` |
| Núcleo verificado | `core/` com SHA-256 idêntico a `verification/results/versoes.txt` (o firmware compila o próprio `core/fsm_core.c`) |
| Pinagem | LEDs carro 25/26/27 · LEDs pedestre VM/VD 18/19 · BTN_PED1 33 · BTN_PED2 32 · RESET 21 · STIM 16/17 |
| Supervisão de linha | `PTT7_SUPERVISAO_HW = 0`: perda de sinal **injetada por software**; GPIO22/23 não exercitados |
| Sessões | 1 (todas as capturas com `sessao=1`); placa sem reinício entre as capturas de falha (~45 min) |

Limitação de rastreabilidade: os logs não contêm o banner de inicialização (a captura começou com a placa já
ligada), então a build usada é **declarada**, não comprovada pelo próprio log.

## Leitura dos resultados

- **Q3 — respondida, protocolo atendido.** 200/200 injeções (4 tipos × 50) chegaram ao estado FALHA com
  verdes apagados dentro da janela de 1 s, sem retorno ao verde. A recuperação via reset deu 200/200.
- **Q2 — respondida de forma exploratória; protocolo NÃO atendido.** Nenhuma medição válida de T4
  (eram necessárias ≥ 30 em ≥ 3 sessões). As causas registradas estão na seção seguinte. Os 23 registros LAT existentes
  (mediana de 14 µs, entre 13 e 18 µs) vêm do mesmo trecho cronometrado, mas sem estímulo controlado. Eles indicam
  apenas a **ordem de grandeza** da latência interna de software e não substituem o protocolo.

### Evidência complementar de Q2: latência lógica (não mede tempo do ESP32)

`tests/test_t4_latencia_logica.c` executa o próprio `core/fsm_core.c` no computador, com o pedido de pedestre
em todos os instantes de 0 a 20 s após o verde mínimo (passos de 10 ms, botões 1 e 2: 4.002 casos).
Resultado: **4.002/4.002 casos com T4 no próprio passo que recebe o pedido** (0 passos extras). No firmware, a
ISR acorda a tarefa de controle diretamente (`vTaskNotifyGiveFromISR`). Nenhum tempo do computador é atribuído
ao ESP32, e o protocolo de Q2 continua não atendido.

```
gcc -std=c99 -O2 -Wall -Wextra -Werror -Icore tests/test_t4_latencia_logica.c core/fsm_core.c -o t4 && ./t4
4002 casos: pior caso = 1 passo(s) da FSM entre o pedido e ATENCAO_VEICULO (T4)
```

### Causas do não atendimento de Q2 (registradas com transparência)

1. O estímulo controlado não chegou ao botão. O firmware registrou 4 descartes do tipo "estímulo não detectado"
   (`s1_lat.log`, seq 0–3). A causa provável é que, após a troca de pinos, o jumper precisava ir de **GPIO16 → GPIO33**
   (a documentação anterior indicava 16 → 18). Essa causa não foi confirmada pela equipe.
2. As duas execuções de `lat 10 5` abortaram ainda no aquecimento ("sistema não voltou a VERDE_VEICULO com verde
   mínimo"): a primeira no ensaio 5 e a segunda no ensaio 1.
3. A segunda captura (11:59) de `s1_lat.log` está corrompida. Cerca de 99,6% do arquivo (1,4 MB) são fragmentos de linha
   repetidos. A causa provável é a porta serial aberta por dois programas ao mesmo tempo. O arquivo foi mantido bruto.
4. Não houve nova rodada de bancada antes do prazo de entrega.

Observações adicionais:
- Houve 17 acionamentos de botão não comandados durante os ensaios de falha (16 em `falha1.log`, 1 em `falha2.log`).
  A origem (toque manual ou ruído) não foi confirmada. Esses acionamentos não alteraram Q3: `verde_apos_falha = 0` em todas as injeções.
- A partir desta versão, `analyze.py` só conta um registro LAT para o protocolo de Q2 quando ele está dentro de um bloco
  `#INICIO lat … #FIM lat`. Essa regra foi definida **depois** de observar os dados e está documentada aqui. Ela não altera Q3.

### Reteste proposto (trabalho futuro)

```bash
# jumpers GPIO16->GPIO33 e GPIO17->GPIO32; feche o idf.py monitor; confira o jumper com o comando "stim 1"
python tools/capture_serial.py --porta <PORTA> --saida data/raw/reteste_s1_lat.log --cmd "sessao 1" --cmd "lat 10 5"
python tools/capture_serial.py --porta <PORTA> --saida data/raw/reteste_s2_lat.log --cmd "sessao 2" --cmd "lat 10 5"
python tools/capture_serial.py --porta <PORTA> --saida data/raw/reteste_s3_lat.log --cmd "sessao 3" --cmd "lat 10 5"
python analysis/analyze.py   # capture_serial.py acrescenta ao arquivo (modo "a"): use nomes novos
```

Gráfico de convergência (Q3): `convergencia_bancada_2026-11-24.png`.
Saída original enviada pela equipe (sem alterações): `bancada_2026-11-24/analysis_out_original/`.

---

## Saída do `analyze.py` (resumo.md)

Arquivos: falha1.log, falha2.log, falha3.log, falha4.log, s1_lat.log, sim_falhas.log
Plataformas consideradas nas falhas: esp32

### Q2 - Latencia interna de software (LREC = ISR -> GPIO atualizado), em us

Registros LAT: 23 | fora de experimento 'lat' (incidentais, nao contam): 17 | aquecimento excluido: 6 | descartados (data/descartes.csv): 0 | validos: 0

| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|

Latencia ate a decisao da FSM (antes de escrever os GPIOs), em us:

| Origem / transicao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|

Por sessao (transicao critica VERDE_VEICULO T4 -> ATENCAO_VEICULO):

| Sessao | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|

Protocolo (>= 30 medicoes validas em >= 3 sessoes para T4): NAO atendido (0 validas, 0 sessoes).
Percentis de cauda (p99) nao sao reportados: amostra insuficiente para evidencia robusta.
Latencia fim a fim (fisica) so pode ser afirmada com analisador logico.

### Q2 - Evidencia exploratoria (NAO atende ao protocolo)

Todos os registros LAT do ESP32, incluindo aquecimento e acionamentos incidentais de botao
durante os ensaios de falha. Mesmo trecho cronometrado (esp_timer_get_time), porem sem estimulo
controlado, sem 3 sessoes e com n pequeno: serve apenas como ordem de grandeza.

| Grupo | n | media | mediana | DP | IIQ (Q1-Q3) | min | max |
|---|---|---|---|---|---|---|---|
| todos (ISR -> GPIO) | 23 | 13.8 | 14.0 | 1.0 | 1.0 (13.0-14.0) | 13.0 | 18.0 |
| dentro do experimento lat (aquecimento) | 6 | 13.7 | 14.0 | 0.5 | 0.8 (13.2-14.0) | 13.0 | 14.0 |
| incidentais (fora do experimento lat) | 17 | 13.8 | 14.0 | 1.2 | 1.0 (13.0-14.0) | 13.0 | 18.0 |
| somente T4 (todas aquecimento) | 2 | 13.5 | 13.5 | 0.7 | 0.5 (13.2-13.8) | 13.0 | 14.0 |

### Q3 - Convergencia ao estado seguro (FALHA, amarelo piscante)

Criterio: verdes apagados e FALHA alcancado em ate 1 s apos a injecao, sem retorno ao verde na janela.

| Tipo de falha | injecoes validas | sucessos | taxa de sucesso | conv. media (ms) | mediana | DP | IIQ | min | max |
|---|---|---|---|---|---|---|---|---|---|
| perda sinal botao 1 | 50 | 50 | 100.0% | 4.67 | 4.43 | 2.86 | 5.00 | 0.42 | 9.43 |
| perda sinal botao 2 | 50 | 50 | 100.0% | 5.67 | 5.43 | 2.63 | 3.75 | 0.42 | 9.43 |
| perda simultanea em VP | 50 | 50 | 100.0% | 4.68 | 4.42 | 2.78 | 5.00 | 0.42 | 9.42 |
| falha de temporizacao | 50 | 50 | 100.0% | 305.22 | 305.43 | 3.13 | 5.50 | 300.42 | 309.43 |

Protocolo (>= 50 injecoes validas por tipo 1-3): ATENDIDO.

### Recuperacao via reset (avaliada separadamente)

| Tipo de falha anterior | ensaios | sucesso | ms ate RECOVERY (mediana) | ms ate VERDE (mediana) |
|---|---|---|---|---|
| perda sinal botao 1 | 50 | 50 | 5.0 | 3005.0 |
| perda sinal botao 2 | 50 | 50 | 5.0 | 3005.0 |
| perda simultanea em VP | 50 | 50 | 5.0 | 3005.0 |
| falha de temporizacao | 50 | 50 | 5.0 | 3005.0 |

### Descartes

Registros #DESCARTE emitidos pelo firmware: 4
- s1_lat.log:5 #DESCARTE,lat,1,0,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)
- s1_lat.log:6 #DESCARTE,lat,1,1,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)
- s1_lat.log:7 #DESCARTE,lat,1,2,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)
- s1_lat.log:8 #DESCARTE,lat,1,3,estimulo nao detectado (verifique jumper STIM1->BTN_PED1)

Graficos: convergencia_boxplot.png
