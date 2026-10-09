/*
 * app_config.h - Ligacao da bancada (ESP32 DevKit V1 / ESP32-WROOM-32) e parametros do invólucro
 * Ver docs/bancada.md para o esquema de ligacao completo.
 */
#pragma once

/* ---- LEDs (saida, ativo em nivel alto, resistor 220-330 ohm em serie) ---- */
#define PIN_CAR_R      25
#define PIN_CAR_Y      26
#define PIN_CAR_G      27
#define PIN_PED_R      32
#define PIN_PED_G      33

/* ---- Botoes (entrada com pull-up interno, ativo em nivel baixo: botao ao GND) ---- */
#define PIN_BTN_PED1   18
#define PIN_BTN_PED2   19
#define PIN_BTN_RESET  21

/* ---- Supervisao da linha dos botoes (entrada com pull-up interno) ----
 * Jumper ao GND = sinal integro (le 0). Retirar o jumper = PERDA DE SINAL (le 1).
 * Se PTT7_SUPERVISAO_HW = 0 os pinos sao ignorados e a perda de sinal so pode
 * ser injetada por software (comandos 'perda'/'falha').                      */
#define PIN_SIG1       22
#define PIN_SIG2       23
#ifndef PTT7_SUPERVISAO_HW
#define PTT7_SUPERVISAO_HW 1
#endif

/* ---- Estimulo controlado (saida open-drain) ----
 * Ligue STIM1 -> BTN_PED1 (GPIO16 -> GPIO18) e STIM2 -> BTN_PED2 (GPIO17 -> GPIO19) com jumper.
 * Em open-drain o pino so puxa para GND; o botao manual continua funcionando.
 * (Placas com modulo WROVER usam 16/17 para PSRAM: troque para 4 e 13.)     */
#define PIN_STIM1      16
#define PIN_STIM2      17

/* ---- Invólucro ---- */
#define CONTROL_PERIOD_MS     10      /* periodo maximo entre passos da FSM          */
#define DEBOUNCE_US        50000      /* antirrepique dos botoes na ISR (50 ms)        */
#define STIM_PULSE_MS         20      /* largura do pulso de estimulo                  */
#define FALLBACK_WINDOW_MS  1000      /* janela de observacao do criterio de fallback  */
#define CTRL_TASK_PRIO        (configMAX_PRIORITIES - 2)
#define CTRL_TASK_CORE         1
#define BENCH_TASK_PRIO        5
#define LOG_TASK_PRIO          3
#define AUX_TASK_CORE          0
