#include <Arduino.h>
#include "fsm_core.h"

// ============================================================
// 1. HARDWARE E MEMÓRIA DOS TESTES
// ============================================================

const uint8_t VR = 25;
const uint8_t VY = 26;
const uint8_t VG = 27;
const uint8_t PR = 18;
const uint8_t PG = 19;

Fsm f;

uint32_t lastMs;
uint32_t startMs;
uint32_t greenStartMs;
uint32_t yellowStartMs;
uint32_t crossStartMs;
uint32_t warningStartMs;
uint32_t recoveryStartMs;
uint32_t initStartMs;
uint32_t injectionMs;

bool channel1 = false;
bool channel2 = false;
bool failed = false;
bool injected = false;
bool finished = false;

bool sawGreen = false;
bool sawYellow = false;
bool sawCross = false;
bool sawWarning = false;
bool sawFault = false;
bool sawRecovery = false;
bool sawInitAfterRecovery = false;

uint8_t scenario = 0;
uint8_t passed = 0;

// Marca eventos já executados para não repeti-los a cada loop.
uint32_t eventMask = 0;

Lights previousLights = {
    false, false, false, false, false
};

const char *const titles[] = {
    "Pedido 1 antecipado: respeitar verde minimo",
    "Pedido 2 tardio: ciclo normal",
    "Pedidos simultaneos: uma unica travessia",
    "Falha canal 1 durante travessia",
    "Falha canal 2 durante travessia",
    "Falha simultanea durante travessia",
    "Atraso de 400 ms"
};

// Declarações explícitas para o editor Arduino.
void outputs(Lights o);
void beginScenario();
void validateTransition(State oldState, uint32_t now);
void conclude();
bool eventAt(uint8_t id, uint32_t when, uint32_t elapsed);
void failCheck(const char *message);

// ============================================================
// 2. ATUALIZAÇÃO E REGISTRO DOS LEDs
// ============================================================

void outputs(Lights o) {
    // Desliga os verdes antes de aplicar a nova combinação.
    digitalWrite(VG, LOW);
    digitalWrite(PG, LOW);

    digitalWrite(VR, o.vr);
    digitalWrite(VY, o.vy);
    digitalWrite(PR, o.pr);

    digitalWrite(VG, o.vg);
    digitalWrite(PG, o.pg);

    // Registra somente mudanças, incluindo cada piscada.
    const bool changed =
        o.vr != previousLights.vr ||
        o.vy != previousLights.vy ||
        o.vg != previousLights.vg ||
        o.pr != previousLights.pr ||
        o.pg != previousLights.pg;

    if (changed) {
        Serial.printf(
            "  LEDs | veiculos R=%d A=%d V=%d"
            " | pedestres R=%d V=%d\n",
            o.vr, o.vy, o.vg, o.pr, o.pg
        );

        previousLights = o;
    }
}

// ============================================================
// 3. CONTROLE DOS EVENTOS E RESULTADOS
// ============================================================

void failCheck(const char *message) {
    failed = true;
    Serial.printf("  [FALHOU] %s\n", message);
}

// Executa um evento apenas uma vez quando seu tempo é atingido.
bool eventAt(
    uint8_t id,
    uint32_t when,
    uint32_t elapsed
) {
    const uint32_t bit = 1UL << id;

    if (elapsed < when || (eventMask & bit)) {
        return false;
    }

    eventMask |= bit;
    return true;
}

// Cada cenário começa com uma FSM independente.
void beginScenario() {
    channel1 = false;
    channel2 = false;
    failed = false;
    injected = false;

    sawGreen = false;
    sawYellow = false;
    sawCross = false;
    sawWarning = false;
    sawFault = false;
    sawRecovery = false;
    sawInitAfterRecovery = false;

    eventMask = 0;
    injectionMs = 0;

    semaforo_fsm_init(&f);

    startMs = millis();
    lastMs = startMs;

    Serial.printf(
        "\n========== TESTE %u/7: %s ==========\n",
        scenario + 1,
        titles[scenario]
    );

    Serial.println(
        "  Preparacao: reinicializacao da FSM;"
        " ambos os sinais vermelhos."
    );

    outputs(semaforo_fsm_lights(&f));
}

// ============================================================
// 4. VERIFICAÇÃO DAS TRANSIÇÕES E TEMPOS
// ============================================================

void validateTransition(State oldState, uint32_t now) {
    if (oldState == f.state) {
        return;
    }

    Serial.printf(
        "  [%lu ms] %s -> %s\n",
        (unsigned long)(now - startMs),
        semaforo_fsm_name(oldState),
        semaforo_fsm_name(f.state)
    );

    if (oldState == FALHA && f.state != RECOVERY) {
        failCheck("Saida de FALHA sem passar por RECOVERY.");
    }

    switch (f.state) {
        case VERDE_VEICULO:
            sawGreen = true;
            greenStartMs = now;

            if (
                oldState == INIT &&
                sawRecovery &&
                now - initStartMs < START_MS
            ) {
                failCheck("INIT terminou antes do tempo minimo.");
            }

            if (oldState == ATENCAO_PEDESTRE) {
                const uint32_t duration = now - warningStartMs;

                Serial.printf(
                    "  Verificacao: aviso dos pedestres"
                    " durou %lu ms.\n",
                    (unsigned long)duration
                );

                if (duration < PED_WARNING_MS) {
                    failCheck("Aviso dos pedestres terminou cedo.");
                }
            }
            break;

        case ATENCAO_VEICULO: {
            sawYellow = true;
            yellowStartMs = now;

            const uint32_t duration = now - greenStartMs;

            Serial.printf(
                "  Verificacao: verde dos veiculos durou"
                " %lu ms; minimo = %d ms.\n",
                (unsigned long)duration,
                MIN_GREEN_MS
            );

            if (duration < MIN_GREEN_MS) {
                failCheck("Verde dos veiculos terminou cedo.");
            }
            break;
        }

        case VERMELHO_VEICULO_VERDE_PEDESTRE: {
            sawCross = true;
            crossStartMs = now;

            const uint32_t duration = now - yellowStartMs;

            Serial.printf(
                "  Verificacao: amarelo durou %lu ms.\n",
                (unsigned long)duration
            );

            if (
                oldState != ATENCAO_VEICULO ||
                duration < YELLOW_MS
            ) {
                failCheck("Travessia sem completar o amarelo.");
            }
            break;
        }

        case ATENCAO_PEDESTRE: {
            sawWarning = true;
            warningStartMs = now;

            const uint32_t duration = now - crossStartMs;

            Serial.printf(
                "  Verificacao: verde fixo dos pedestres"
                " durou %lu ms.\n",
                (unsigned long)duration
            );

            if (duration < CROSS_MS) {
                failCheck("Travessia terminou cedo.");
            }
            break;
        }

        case FALHA:
            sawFault = true;

            if (injected) {
                const uint32_t convergence = now - injectionMs;

                Serial.printf(
                    "  Verificacao: FALHA alcancada %lu ms"
                    " apos injecao; limite = 1000 ms.\n",
                    (unsigned long)convergence
                );

                if (convergence > 1000) {
                    failCheck("Convergencia excedeu 1 segundo.");
                }
            } else {
                failCheck("Falha inesperada, sem injecao.");
            }
            break;

        case RECOVERY:
            sawRecovery = true;
            recoveryStartMs = now;
            break;

        case INIT:
            if (oldState == RECOVERY) {
                sawInitAfterRecovery = true;
                initStartMs = now;

                if (now - recoveryStartMs < RECOVERY_MS) {
                    failCheck("RECOVERY terminou cedo.");
                }
            }
            break;

        default:
            break;
    }
}

// ============================================================
// 5. ENCERRAMENTO DE CADA CENÁRIO
// ============================================================

void conclude() {
    const bool normal = scenario < 3;

    if (
        normal &&
        !(
            sawGreen &&
            sawYellow &&
            sawCross &&
            sawWarning &&
            f.state == VERDE_VEICULO
        )
    ) {
        failCheck("Ciclo normal incompleto.");
    }

    if (
        !normal &&
        !(
            sawFault &&
            sawRecovery &&
            sawInitAfterRecovery &&
            f.state == VERDE_VEICULO
        )
    ) {
        failCheck("Sequencia de falha ou recuperacao incompleta.");
    }

    if (
        scenario >= 3 &&
        scenario <= 5 &&
        !sawCross
    ) {
        failCheck("Falha nao foi injetada durante a travessia.");
    }

    Serial.printf(
        "RESULTADO TESTE %u: %s\n",
        scenario + 1,
        failed ? "REPROVADO" : "APROVADO"
    );

    if (!failed) {
        passed++;
    }

    scenario++;

    if (scenario == 7) {
        finished = true;

        Serial.printf(
            "\n========== FINAL: %u/7 APROVADOS ==========\n",
            passed
        );

        Serial.println(
            "Resultados funcionais da simulacao."
            " Nao equivalem a prova formal ou medicao fisica."
        );

        Serial.println(
            "Reinicie a simulacao para repetir a bateria."
        );
    } else {
        beginScenario();
    }
}

// ============================================================
// 6. INICIALIZAÇÃO
// ============================================================

void setup() {
    Serial.begin(115200);

    const uint8_t pins[] = {VR, VY, VG, PR, PG};

    for (uint8_t pin : pins) {
        pinMode(pin, OUTPUT);
    }

    Serial.println("BATERIA AUTOMATICA DO SEMAFORO");
    Serial.println("Duracao aproximada: 100 segundos simulados.");

    Serial.println(
        "Entradas artificiais: botoes fisicos e comandos"
        " do monitor serial nao sao usados neste modo."
    );

    Serial.println(
        "Legenda dos LEDs: R=vermelho, A=amarelo,"
        " V=verde; 1=ligado, 0=desligado."
    );

    beginScenario();
}

// ============================================================
// 7. EXECUÇÃO AUTOMÁTICA
// ============================================================

void loop() {
    if (finished) {
        delay(100);
        return;
    }

    uint32_t now = millis();
    const uint32_t elapsed = now - startMs;

    bool request = false;
    bool reset = false;

    // --------------------------------------------------------
    // Cenários 1 a 3: operação normal.
    // --------------------------------------------------------

    if (scenario < 3) {
        // Pedido tardio aos 6,5 s; demais aos 1,5 s.
        const uint32_t requestTime =
            scenario == 1 ? 6500 : 1500;

        if (eventAt(0, requestTime, elapsed)) {
            request = true;

            const char *description =
                scenario == 0
                    ? "PEDIDO 1 antecipado"
                    : scenario == 1
                        ? "PEDIDO 2 tardio"
                        : "PEDIDOS 1 e 2 simultaneos, agrupados";

            Serial.printf(
                "  ACAO: %s. Pedido gerado uma vez.\n",
                description
            );
        }
    }

    // --------------------------------------------------------
    // Cenários 4 a 6: falhas durante a travessia.
    // --------------------------------------------------------

    else if (scenario <= 5) {
        if (eventAt(0, 1500, elapsed)) {
            request = true;

            Serial.println(
                "  ACAO: solicitar travessia"
                " para preparar a injecao."
            );
        }

        if (
            !injected &&
            f.state == VERMELHO_VEICULO_VERDE_PEDESTRE &&
            f.elapsed >= 1000
        ) {
            injected = true;
            injectionMs = now;

            channel1 = scenario == 3 || scenario == 5;
            channel2 = scenario == 4 || scenario == 5;

            Serial.printf(
                "  INJECAO: falha artificial durante travessia."
                " Canal 1=%d; canal 2=%d.\n",
                channel1,
                channel2
            );
        }
    }

    // --------------------------------------------------------
    // Cenário 7: atraso real de 400 ms no programa.
    // --------------------------------------------------------

    else if (!injected && elapsed >= 2500) {
        injected = true;
        injectionMs = now;

        Serial.println(
            "  INJECAO: pausa de 400 ms."
            " A falha sera detectada quando o controle voltar."
        );

        delay(400);
        now = millis();
    }

    // --------------------------------------------------------
    // Testes de bloqueio e recuperação após cada falha.
    // --------------------------------------------------------

    if (injected) {
        const uint32_t since = now - injectionMs;

        if (eventAt(1, 1100, since)) {
            request = true;

            Serial.println(
                "  ACAO: pedido durante FALHA."
                " Deve continuar bloqueado."
            );
        }

        if (eventAt(2, 1500, since)) {
            // Nos cenários de canal, a falha continua ativa.
            // No cenário de atraso, ela foi um evento pontual.
            if (scenario != 6) {
                reset = true;

                Serial.println(
                    "  ACAO: reset com falha ativa."
                    " Deve permanecer em FALHA."
                );
            }
        }

        if (eventAt(3, 2000, since)) {
            channel1 = false;
            channel2 = false;

            Serial.println(
                "  ACAO: retirar falhas SEM reset."
                " Deve permanecer em FALHA."
            );
        }

        if (eventAt(4, 3000, since)) {
            reset = true;

            Serial.println(
                "  ACAO: reset apos retirada da falha."
                " Esperado: RECOVERY -> INIT -> VERDE."
            );
        }
    }

    // --------------------------------------------------------
    // Executa o mesmo núcleo C usado no modo manual.
    // --------------------------------------------------------

    const State oldState = f.state;
    const uint32_t dt = now - lastMs;
    lastMs = now;

    semaforo_fsm_step(
        &f,
        dt,
        request,
        reset,
        channel1 || channel2
    );

    const Lights lights = semaforo_fsm_lights(&f);

    // Aplica os LEDs antes de emitir os registros de transição.
    outputs(lights);
    validateTransition(oldState, now);

    // Verifica exclusão mútua em todas as etapas executadas.
    if (lights.vg && lights.pg) {
        failCheck("Dois verdes simultaneos.");
    }

    // Durante a janela anterior ao reset autorizado,
    // exige FALHA mantida e ambos os verdes desligados.
    if (injected && now - injectionMs < 3000) {
        if (
            f.state != FALHA ||
            lights.vg ||
            lights.pg
        ) {
            failCheck(
                "Falha nao permaneceu bloqueada"
                " ate o reset autorizado."
            );
        }
    }

    // --------------------------------------------------------
    // Detecta conclusão ou tempo máximo excedido.
    // --------------------------------------------------------

    if (
        scenario < 3 &&
        sawWarning &&
        f.state == VERDE_VEICULO
    ) {
        conclude();
    } else if (
        scenario >= 3 &&
        sawInitAfterRecovery &&
        f.state == VERDE_VEICULO
    ) {
        conclude();
    } else if (now - startMs > 22000) {
        failCheck("Timeout: cenario excedeu 22 segundos.");
        conclude();
    }

    delay(10);
}