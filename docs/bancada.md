# Montagem da bancada (ESP32 DevKit V1 / ESP32-WROOM-32)

## Material

| Item | Qtd | Observação |
|---|---|---|
| Placa ESP32 DevKit V1 (WROOM-32) | 1 | com WROVER, troque STIM1/STIM2 para GPIO 4/13 em `firmware/main/app_config.h` |
| Protoboard + jumpers macho-macho | 1 + ~20 | |
| LED vermelho, amarelo, verde (veículos) | 3 | |
| LED vermelho, verde (pedestres) | 2 | |
| Resistor 220–330 Ω | 5 | um em série com cada LED |
| Botão táctil (push-button) | 3 | pedestre 1, pedestre 2, reset |
| Cabo USB de dados | 1 | |
| Analisador lógico (opcional) | 1 | para latência física entrada→saída |

## Ligações

Pinagem conforme `firmware/main/app_config.h` (usada nos ensaios de 24/11/2026).

| Função | GPIO | Ligação |
|---|---|---|
| LED veículo VERMELHO | 25 | GPIO → resistor → ânodo do LED; cátodo → GND |
| LED veículo AMARELO | 26 | idem |
| LED veículo VERDE | 27 | idem |
| LED pedestre VERMELHO | 18 | idem |
| LED pedestre VERDE | 19 | idem |
| Botão pedestre 1 | 33 | um terminal no GPIO, outro no GND (pull-up interno) |
| Botão pedestre 2 | 32 | idem |
| Botão reset (da FSM) | 21 | idem — **não** é o botão EN/RST da placa |
| Supervisão do sinal do botão 1 | 22 | **só com `PTT7_SUPERVISAO_HW=1`**: jumper ao GND = sinal íntegro; retirar = perda de sinal. Com HW=0 (padrão) o pino é ignorado e **não** leva jumper |
| Supervisão do sinal do botão 2 | 23 | idem (somente com HW=1) |
| Estímulo controlado 1 | 16 | jumper **GPIO16 → GPIO33** (necessário para `lat`; confira com `stim 1`, MANUAL seção 8.1) |
| Estímulo controlado 2 | 17 | jumper **GPIO17 → GPIO32** (opcional, `lat n 5 2`) |

```
             ESP32 DevKit V1
          +------------------+
  LED VM--| 25           GND |--+-- GND comum (catodos, botoes)
  LED AM--| 26            23 |   (SIG2: ignorado com HW=0; jumper ao GND so com HW=1)
  LED VD--| 27            22 |   (SIG1: ignorado com HW=0; jumper ao GND so com HW=1)
 PED VM---| 18            21 |---[BOTAO RESET]--- GND
 PED VD---| 19            33 |---[BOTAO PED 1]--- GND  <-- jumper vindo de GPIO16
          |               32 |---[BOTAO PED 2]--- GND  <-- jumper vindo de GPIO17 (opcional)
          |               17 |--> (para GPIO32)
          |               16 |--> (para GPIO33)
          +------------------+
  cada LED: GPIO -> 220R -> LED -> GND
```

> **Supervisão do sinal:** com `PTT7_SUPERVISAO_HW=0` (padrão em `app_config.h`, usado em 24/11/2026)
> os GPIO22/23 são ignorados e **não precisam de jumper**; a perda de sinal é injetada por software
> pelos comandos `perda 1`, `perda 2`, `perda 12` e `restaura` (e `falha <tipo> <n>` para ensaios
> repetidos; `atraso <ms>` injeta falha de temporização). Modo opcional: trocando a linha
> `#define PTT7_SUPERVISAO_HW 0` por `1` em `firmware/main/app_config.h` e gravando de novo (passar
> `-DPTT7_SUPERVISAO_HW=1` ao `idf.py` não chega ao código C), os jumpers de GPIO22 e GPIO23 ao GND
> passam a ser necessários; sem eles
> o firmware interpreta perda de sinal e vai direto para FALHA (amarelo piscante), e retirar o jumper
> passa a ser a forma física de provocar a perda. O caminho físico GPIO22/23 não foi exercitado em 24/11/2026.

## Analisador lógico (opcional, latência física)

- Canal 0: GPIO33 (botão/estímulo pedestre 1, borda de descida).
- Canal 1: GPIO26 (LED amarelo veicular, borda de subida na transição T4).
- Latência física = t(subida CH1) − t(descida CH0). Sem analisador, o relatório só afirma a
  latência **interna de software** (`esp_timer_get_time()` da ISR até o GPIO).

## Configuração fixa (registrar no relatório)

CPU 240 MHz · FreeRTOS 1000 Hz · período de controle 10 ms · tarefa de controle no núcleo 1,
prioridade `configMAX_PRIORITIES-2` · ISR em IRAM · núcleo da FSM em IRAM · UART 115200 ·
alimentação pela USB do mesmo computador em todas as sessões.

## Configuração usada nos ensaios de 24/11/2026

- Pinagem da tabela acima; `PTT7_SUPERVISAO_HW=0` (sem jumpers em GPIO22/23; perda de sinal por software).
- Jumpers de estímulo corretos para `lat`: GPIO16 → GPIO33 e GPIO17 → GPIO32.
- Plataforma: ESP-IDF 5.1.4 · CPU 240 MHz · FreeRTOS 1000 Hz · núcleo da FSM em IRAM · UART 115200.
- Com HW=1 seriam necessários os jumpers de GPIO22/23 ao GND (modo opcional, não usado nesses ensaios).
- Resultados: `docs/validacao/evidencias/resultados_bancada_2026-11-24.md`.
