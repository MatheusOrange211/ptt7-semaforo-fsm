#ifndef FSM_CORE_H
#define FSM_CORE_H

#include <stdint.h>
#include <stdbool.h>

// Permite chamar as funções C pelo código C++ do Arduino.
#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// ESTADOS DO SEMÁFORO
// ============================================================

typedef enum {
    INIT,                              // Inicialização: ambos vermelhos.
    VERDE_VEICULO,                      // Veículos liberados.
    PEDIDO_PEDESTRE,                   // Pedido registrado; aguarda verde mínimo.
    ATENCAO_VEICULO,                    // Amarelo dos veículos.
    VERMELHO_VEICULO_VERDE_PEDESTRE,    // Travessia liberada.
    ATENCAO_PEDESTRE,                   // Verde dos pedestres piscando.
    FALHA,                             // Verdes desligados; amarelo piscando.
    RECOVERY                           // Recuperação antes de reinicializar.
} State;

// ============================================================
// MEMÓRIA DA MÁQUINA DE ESTADOS
// ============================================================

typedef struct {
    State state;             // Estado atual.
    uint32_t elapsed;        // Tempo no estado atual, em ms.
    uint32_t green_elapsed;  // Tempo da fase verde dos veículos, em ms.
} Fsm;

// ============================================================
// SAÍDAS DOS LEDs
// ============================================================

// true = ligado; false = desligado.
typedef struct {
    bool vr;  // Veículos: vermelho.
    bool vy;  // Veículos: amarelo.
    bool vg;  // Veículos: verde.
    bool pr;  // Pedestres: vermelho.
    bool pg;  // Pedestres: verde.
} Lights;

// ============================================================
// TEMPOS DIDÁTICOS, EM MILISSEGUNDOS
// ============================================================

enum {
    START_MS       = 1000,
    MIN_GREEN_MS   = 5000,
    YELLOW_MS      = 2000,
    CROSS_MS       = 5000,
    PED_WARNING_MS = 2000,
    RECOVERY_MS    = 1000
};

// ============================================================
// INTERFACE DO NÚCLEO
// ============================================================

// Inicializa em INIT e zera os temporizadores.
void semaforo_fsm_init(Fsm *f);

// Executa uma etapa sem acessar o hardware.
//
// dt_ms:   tempo desde a chamada anterior.
// request: evento de pedido de travessia.
// reset:   evento de acionamento do reset.
// fault:   indicador de falha ativa.
void semaforo_fsm_step(
    Fsm *f,
    uint32_t dt_ms,
    bool request,
    bool reset,
    bool fault
);

// Calcula os LEDs sem modificar a máquina ou os GPIOs.
Lights semaforo_fsm_lights(const Fsm *f);

// Retorna o nome do estado para diagnóstico.
const char *semaforo_fsm_name(State estado);

#ifdef __cplusplus
}
#endif

#endif /* FSM_CORE_H */