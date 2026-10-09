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

| Função | GPIO | Ligação |
|---|---|---|
| LED veículo VERMELHO | 25 | GPIO → resistor → ânodo do LED; cátodo → GND |
| LED veículo AMARELO | 26 | idem |
| LED veículo VERDE | 27 | idem |
| LED pedestre VERMELHO | 32 | idem |
| LED pedestre VERDE | 33 | idem |
| Botão pedestre 1 | 18 | um terminal no GPIO, outro no GND (pull-up interno) |
| Botão pedestre 2 | 19 | idem |
| Botão reset (da FSM) | 21 | idem — **não** é o botão EN/RST da placa |
| Supervisão do sinal do botão 1 | 22 | **jumper ao GND** = sinal íntegro; retirar = perda de sinal |
| Supervisão do sinal do botão 2 | 23 | **jumper ao GND** = sinal íntegro; retirar = perda de sinal |
| Estímulo controlado 1 | 16 | jumper **GPIO16 → GPIO18** (necessário para `lat`) |
| Estímulo controlado 2 | 17 | jumper **GPIO17 → GPIO19** (opcional, `lat n 5 2`) |

```
             ESP32 DevKit V1
          +------------------+
  LED VM--| 25           GND |--+-- GND comum (catodos, botoes, jumpers SIG)
  LED AM--| 26            23 |--+ jumper SIG2 ao GND
  LED VD--| 27            22 |--+ jumper SIG1 ao GND
 PED VM---| 32            21 |---[BOTAO RESET]--- GND
 PED VD---| 33            19 |---[BOTAO PED 2]--- GND  <-- jumper de GPIO17 (opcional)
          |               18 |---[BOTAO PED 1]--- GND  <-- jumper de GPIO16
          |               17 |--> (para GPIO19)
          |               16 |--> (para GPIO18)
          +------------------+
  cada LED: GPIO -> 220R -> LED -> GND
```

> **Atenção:** se os jumpers de supervisão (GPIO22 e GPIO23 ao GND) não estiverem colocados,
> o firmware interpreta **perda de sinal** e vai direto para FALHA (amarelo piscante). Esse é o
> comportamento correto e seguro. Para testar sem eles, compile com `-DPTT7_SUPERVISAO_HW=0`
> (ver `app_config.h`); a perda de sinal passa a ser só por software.

## Analisador lógico (opcional, latência física)

- Canal 0: GPIO18 (botão/estímulo pedestre 1, borda de descida).
- Canal 1: GPIO26 (LED amarelo veicular, borda de subida na transição T4).
- Latência física = t(subida CH1) − t(descida CH0). Sem analisador, o relatório só afirma a
  latência **interna de software** (`esp_timer_get_time()` da ISR até o GPIO).

## Configuração fixa (registrar no relatório)

CPU 240 MHz · FreeRTOS 1000 Hz · período de controle 10 ms · tarefa de controle no núcleo 1,
prioridade `configMAX_PRIORITIES-2` · ISR em IRAM · núcleo da FSM em IRAM · UART 115200 ·
alimentação pela USB do mesmo computador em todas as sessões.
