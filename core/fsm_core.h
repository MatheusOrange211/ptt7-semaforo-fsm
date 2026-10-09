/*
 * fsm_core.h - Nucleo funcional da FSM do semaforo com travessia de pedestres
 * PTT7 - Arquitetura e Organizacao de Computadores - PPCA/UFRR
 *
 * Regras de projeto do nucleo:
 *   - C99 puro, sem chamadas de hardware, sem alocacao dinamica, sem estado global;
 *   - o tempo decorrido entra EXPLICITAMENTE em cada passo (inputs.dt_ms);
 *   - o MESMO arquivo e compilado no firmware ESP32, nos testes de host,
 *     no simulador (Plano B) e analisado pelo ESBMC.
 */
#ifndef FSM_CORE_H
#define FSM_CORE_H

#include <stdint.h>
#include "fsm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Estados (8) ------------------------------------------------------ */
typedef enum {
    ST_INIT = 0,
    ST_VERDE_VEICULO = 1,
    ST_PEDIDO_PEDESTRE = 2,
    ST_ATENCAO_VEICULO = 3,
    ST_VERMELHO_VEICULO_VERDE_PEDESTRE = 4,
    ST_ATENCAO_PEDESTRE = 5,
    ST_FALHA = 6,
    ST_RECOVERY = 7,
    FSM_NUM_STATES = 8
} fsm_state_t;

/* ----- Identificadores das transicoes (16 + defensiva) ------------------ */
enum {
    TR_NONE = 0,         /* permanece no estado (laco implicito)              */
    TR_T1 = 1,           /* INIT -> VERDE_VEICULO                             */
    TR_T2,               /* INIT -> FALHA                                     */
    TR_T3,               /* VERDE_VEICULO -> PEDIDO_PEDESTRE                  */
    TR_T4,               /* VERDE_VEICULO -> ATENCAO_VEICULO                  */
    TR_T5,               /* VERDE_VEICULO -> FALHA                            */
    TR_T6,               /* PEDIDO_PEDESTRE -> ATENCAO_VEICULO                */
    TR_T7,               /* PEDIDO_PEDESTRE -> FALHA                          */
    TR_T8,               /* ATENCAO_VEICULO -> VERMELHO_VEICULO_VERDE_PED.    */
    TR_T9,               /* ATENCAO_VEICULO -> FALHA                          */
    TR_T10,              /* VERMELHO_VEIC_VERDE_PED -> ATENCAO_PEDESTRE       */
    TR_T11,              /* VERMELHO_VEIC_VERDE_PED -> FALHA                  */
    TR_T12,              /* ATENCAO_PEDESTRE -> VERDE_VEICULO                 */
    TR_T13,              /* ATENCAO_PEDESTRE -> FALHA                         */
    TR_T14,              /* FALHA -> RECOVERY                                 */
    TR_T15,              /* RECOVERY -> VERDE_VEICULO                         */
    TR_T16,              /* RECOVERY -> FALHA                                 */
    TR_DEFENSIVE = 99    /* estado corrompido/invalido -> FALHA               */
};

/* ----- Contexto (estado interno do nucleo) ------------------------------ */
typedef struct {
    uint8_t  state;       /* fsm_state_t                                      */
    uint8_t  pending;     /* demanda de travessia memorizada (latch)          */
    uint8_t  last_tr;     /* ultima transicao disparada (TR_*)                */
    uint8_t  _pad;
    uint32_t elapsed_ms;  /* tempo no estado atual (saturado)                 */
    uint32_t green_ms;    /* tempo acumulado de verde veicular (VV + PP)      */
} fsm_ctx_t;

/* ----- Entradas de um passo --------------------------------------------- */
typedef struct {
    uint8_t  req1;        /* botao de pedestre 1 acionado (evento)            */
    uint8_t  req2;        /* botao de pedestre 2 acionado (evento)            */
    uint8_t  sig1_ok;     /* supervisao da linha do botao 1 integra           */
    uint8_t  sig2_ok;     /* supervisao da linha do botao 2 integra           */
    uint8_t  reset;       /* botao de reset acionado (evento)                 */
    uint8_t  _pad[3];
    uint32_t dt_ms;       /* tempo decorrido desde o passo anterior           */
} fsm_inputs_t;

/* ----- Saidas (LEDs) ----------------------------------------------------- */
typedef struct {
    uint8_t car_r, car_y, car_g;  /* semaforo veicular                        */
    uint8_t ped_r, ped_g;         /* semaforo de pedestres                    */
} fsm_outputs_t;

void        fsm_init(fsm_ctx_t *ctx);
uint8_t     fsm_step(fsm_ctx_t *ctx, const fsm_inputs_t *in);   /* retorna TR_* */
void        fsm_outputs(const fsm_ctx_t *ctx, fsm_outputs_t *out);
int         fsm_input_fault(const fsm_inputs_t *in);            /* 1 = violacao */
int         fsm_ctx_valid(const fsm_ctx_t *ctx);
const char *fsm_state_name(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* FSM_CORE_H */
