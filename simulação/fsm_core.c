#include "fsm_core.h"

// ============================================================
// PARÂMETROS INTERNOS
// ============================================================

enum {
    // Intervalo máximo admissível entre chamadas do núcleo.
    MAX_STEP_MS = 250,

    // Cada valor é a duração de metade do ciclo piscante.
    PED_BLINK_HALF_PERIOD_MS   = 250,
    FAULT_BLINK_HALF_PERIOD_MS = 500
};

// ============================================================
// FUNÇÕES AUXILIARES
// ============================================================

// Evita que o temporizador volte a zero por overflow.
// Ao atingir o limite, permanece em UINT32_MAX.
static uint32_t somarComSaturacao(uint32_t a, uint32_t b) {
    if (b > UINT32_MAX - a) {
        return UINT32_MAX;
    }

    return a + b;
}

// Troca o estado e reinicia a contagem de tempo desse estado.
static void entrarNoEstado(Fsm *f, State novoEstado) {
    f->state = novoEstado;
    f->elapsed = 0;

    // O pedido de pedestre não reinicia o tempo mínimo de verde.
    // Esse contador só zera quando começa uma nova fase verde.
    if (novoEstado == VERDE_VEICULO) {
        f->green_elapsed = 0;
    }
}

// ============================================================
// INICIALIZAÇÃO
// ============================================================

void semaforo_fsm_init(Fsm *f) {
    f->state = INIT;
    f->elapsed = 0;
    f->green_elapsed = 0;
}

// ============================================================
// TRANSIÇÕES DA MÁQUINA DE ESTADOS
// ============================================================

void semaforo_fsm_step(
    Fsm *f,
    uint32_t dt_ms,
    bool request,
    bool reset,
    bool fault
) {
    // 1. Atualiza os temporizadores.
    f->elapsed = somarComSaturacao(f->elapsed, dt_ms);

    if (
        f->state == VERDE_VEICULO ||
        f->state == PEDIDO_PEDESTRE
    ) {
        f->green_elapsed = somarComSaturacao(
            f->green_elapsed,
            dt_ms
        );
    }

    // 2. Falhas têm prioridade sobre reset e operação normal.
    const bool estadoInvalido =
        (unsigned)f->state > (unsigned)RECOVERY;

    const bool falhaTemporizacao =
        dt_ms > MAX_STEP_MS;

    if (fault || falhaTemporizacao || estadoInvalido) {
        if (f->state != FALHA) {
            entrarNoEstado(f, FALHA);
        }

        // Se já estiver em FALHA, preserva o tempo para
        // manter o amarelo piscando.
        return;
    }

    // 3. Reset sem falha ativa inicia a recuperação.
    if (reset && f->state != RECOVERY) {
        entrarNoEstado(f, RECOVERY);
        return;
    }

    // 4. Avalia as transições do estado atual.
    switch (f->state) {
        case INIT:
            // Ambos vermelhos durante a inicialização.
            if (f->elapsed >= START_MS) {
                entrarNoEstado(f, VERDE_VEICULO);
            }
            break;

        case VERDE_VEICULO:
            // Registra o pedido mantendo o verde dos veículos.
            if (request) {
                entrarNoEstado(f, PEDIDO_PEDESTRE);
            }
            break;

        case PEDIDO_PEDESTRE:
            // O pedido permanece registrado após soltar o botão.
            // Aguarda o tempo mínimo da fase verde.
            if (f->green_elapsed >= MIN_GREEN_MS) {
                entrarNoEstado(f, ATENCAO_VEICULO);
            }
            break;

        case ATENCAO_VEICULO:
            // Após o amarelo, libera os pedestres.
            if (f->elapsed >= YELLOW_MS) {
                entrarNoEstado(
                    f,
                    VERMELHO_VEICULO_VERDE_PEDESTRE
                );
            }
            break;

        case VERMELHO_VEICULO_VERDE_PEDESTRE:
            // Após o verde fixo, inicia o aviso piscante.
            if (f->elapsed >= CROSS_MS) {
                entrarNoEstado(f, ATENCAO_PEDESTRE);
            }
            break;

        case ATENCAO_PEDESTRE:
            // Encerra a travessia e libera os veículos.
            if (f->elapsed >= PED_WARNING_MS) {
                entrarNoEstado(f, VERDE_VEICULO);
            }
            break;

        case FALHA:
            // Permanece travado até reset sem falha ativa.
            // O reset é tratado antes deste switch.
            break;

        case RECOVERY:
            // Retorna à inicialização, sem liberar verde direto.
            if (f->elapsed >= RECOVERY_MS) {
                entrarNoEstado(f, INIT);
            }
            break;

        default:
            entrarNoEstado(f, FALHA);
            break;
    }

    // Sem condição de transição, mantém o estado atual.
    // Pedidos fora de VERDE_VEICULO não são enfileirados.
}

// ============================================================
// CÁLCULO DOS LEDs
// ============================================================

Lights semaforo_fsm_lights(const Fsm *f) {
    // Inicialmente, apenas o vermelho dos pedestres está ligado.
    Lights sinais = {
        .vr = false,
        .vy = false,
        .vg = false,
        .pr = true,
        .pg = false
    };

    switch (f->state) {
        case VERDE_VEICULO:
        case PEDIDO_PEDESTRE:
            sinais.vg = true;
            break;

        case ATENCAO_VEICULO:
            sinais.vy = true;
            break;

        case VERMELHO_VEICULO_VERDE_PEDESTRE:
            sinais.vr = true;
            sinais.pr = false;
            sinais.pg = true;
            break;

        case ATENCAO_PEDESTRE:
            sinais.vr = true;
            sinais.pr = false;

            // Alterna o verde dos pedestres a cada 250 ms.
            sinais.pg =
                (f->elapsed / PED_BLINK_HALF_PERIOD_MS) % 2 == 0;
            break;

        case FALHA:
            // Verdes desligados; vermelho dos pedestres fixo.
            // Alterna o amarelo dos veículos a cada 500 ms.
            sinais.vy =
                (f->elapsed / FAULT_BLINK_HALF_PERIOD_MS) % 2 == 0;
            break;

        case INIT:
        case RECOVERY:
        default:
            // Ambos vermelhos.
            sinais.vr = true;
            break;
    }

    return sinais;
}

// ============================================================
// NOMES DOS ESTADOS
// ============================================================

const char *semaforo_fsm_name(State estado) {
    static const char *const nomes[] = {
        "INIT",
        "VERDE_VEICULO",
        "PEDIDO_PEDESTRE",
        "ATENCAO_VEICULO",
        "VERMELHO_VEICULO_VERDE_PEDESTRE",
        "ATENCAO_PEDESTRE",
        "FALHA",
        "RECOVERY"
    };

    if ((unsigned)estado > (unsigned)RECOVERY) {
        return "INVALIDO";
    }

    return nomes[(unsigned)estado];
}