/*
 * fsm_core.c - Funcao de transicao e funcao de saida da FSM do semaforo
 * PTT7 - PPCA/UFRR
 *
 * Este arquivo NAO faz acesso a hardware. A implementacao segue linha a linha
 * a tabela spec/transition_table.csv (T1..T16). Qualquer alteracao aqui deve
 * ser refletida na tabela e no modelo Z3 (verification/z3/fsm_model.hpp).
 *
 * Mutante para teste negativo da verificacao: compile com -DFSM_INJECT_BUG
 * (FALHA vai direto para VERMELHO_VEICULO_VERDE_PEDESTRE no reset, violando P6
 * e pulando RECOVERY). Use apenas para demonstrar que ESBMC/testes detectam.
 */
#include "fsm_core.h"

/* Soma saturada SEM overflow aritmetico (versao anterior usava o wrap-around de
 * uint32 e foi refutada pelo ESBMC com --unsigned-overflow-check; ver
 * docs/registro_verificacao.md, achado ESBMC-01). */
static uint32_t sat_add(uint32_t a, uint32_t b)
{
    if (a >= FSM_TIMER_SAT_MS || b >= (uint32_t)FSM_TIMER_SAT_MS - a) {
        return FSM_TIMER_SAT_MS;
    }
    return a + b;
}

int fsm_input_fault(const fsm_inputs_t *in)
{
    return (!in->sig1_ok) || (!in->sig2_ok) || (in->dt_ms > FSM_DT_MAX_MS);
}

int fsm_ctx_valid(const fsm_ctx_t *ctx)
{
    return ctx->state < FSM_NUM_STATES
        && ctx->pending <= 1
        && ctx->elapsed_ms <= FSM_TIMER_SAT_MS
        && ctx->green_ms <= FSM_TIMER_SAT_MS;
}

void fsm_init(fsm_ctx_t *ctx)
{
    ctx->state = ST_INIT;
    ctx->pending = 0;
    ctx->last_tr = TR_NONE;
    ctx->_pad = 0;
    ctx->elapsed_ms = 0;
    ctx->green_ms = 0;
}

uint8_t fsm_step(fsm_ctx_t *ctx, const fsm_inputs_t *in)
{
    const int fault = fsm_input_fault(in);
    const int req = (in->req1 != 0) || (in->req2 != 0);
    const uint8_t cur = ctx->state;
    uint8_t next = cur;
    uint8_t tr = TR_NONE;

    /* 1) Avanca os temporizadores com o tempo decorrido informado. */
    ctx->elapsed_ms = sat_add(ctx->elapsed_ms, in->dt_ms);
    if (cur == ST_VERDE_VEICULO || cur == ST_PEDIDO_PEDESTRE) {
        ctx->green_ms = sat_add(ctx->green_ms, in->dt_ms);
    }

    /* 2) Memoriza demanda de travessia (exceto quando ja atendida ou em falha). */
    if (req && cur != ST_VERMELHO_VEICULO_VERDE_PEDESTRE && cur != ST_FALHA) {
        ctx->pending = 1;
    }
    const int demand = (ctx->pending != 0);

    /* 3) Funcao de transicao (tabela T1..T16). Falha tem prioridade. */
    switch (cur) {
    case ST_INIT:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T2;  }
        else if (ctx->elapsed_ms >= FSM_T_INIT_MS)   { next = ST_VERDE_VEICULO;  tr = TR_T1;  }
        break;

    case ST_VERDE_VEICULO:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T5;  }
        else if (demand && ctx->green_ms >= FSM_T_VERDE_MIN_MS)
                                                     { next = ST_ATENCAO_VEICULO; tr = TR_T4; }
        else if (demand)                             { next = ST_PEDIDO_PEDESTRE; tr = TR_T3; }
        break;

    case ST_PEDIDO_PEDESTRE:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T7;  }
        else if (ctx->green_ms >= FSM_T_VERDE_MIN_MS){ next = ST_ATENCAO_VEICULO; tr = TR_T6; }
        break;

    case ST_ATENCAO_VEICULO:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T9;  }
        else if (ctx->elapsed_ms >= FSM_T_AMARELO_MS){ next = ST_VERMELHO_VEICULO_VERDE_PEDESTRE; tr = TR_T8; }
        break;

    case ST_VERMELHO_VEICULO_VERDE_PEDESTRE:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T11; }
        else if (ctx->elapsed_ms >= FSM_T_VERDE_PED_MS)
                                                     { next = ST_ATENCAO_PEDESTRE; tr = TR_T10; }
        break;

    case ST_ATENCAO_PEDESTRE:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T13; }
        else if (ctx->elapsed_ms >= FSM_T_ATENCAO_PED_MS)
                                                     { next = ST_VERDE_VEICULO;  tr = TR_T12; }
        break;

    case ST_FALHA:
#ifdef FSM_INJECT_BUG
        if (in->reset && !fault)                     { next = ST_VERMELHO_VEICULO_VERDE_PEDESTRE; tr = TR_T14; }
#else
        if (in->reset && !fault)                     { next = ST_RECOVERY;       tr = TR_T14; }
#endif
        break;

    case ST_RECOVERY:
        if (fault)                                   { next = ST_FALHA;          tr = TR_T16; }
        else if (ctx->elapsed_ms >= FSM_T_RECOVERY_MS)
                                                     { next = ST_VERDE_VEICULO;  tr = TR_T15; }
        break;

    default:
        /* Estado corrompido: programacao defensiva leva ao estado seguro. */
        next = ST_FALHA;
        tr = TR_DEFENSIVE;
        break;
    }

    /* 4) Efeitos de entrada no novo estado. */
    if (tr != TR_NONE) {
        ctx->state = next;
        ctx->elapsed_ms = 0;
        if (next == ST_VERDE_VEICULO && cur != ST_VERDE_VEICULO) {
            ctx->green_ms = 0;
        }
        if (next == ST_VERMELHO_VEICULO_VERDE_PEDESTRE || next == ST_FALHA) {
            ctx->pending = 0;   /* demanda atendida ou descartada */
        }
        if (next == ST_FALHA) {
            ctx->green_ms = 0;
        }
    }
    ctx->last_tr = tr;
    return tr;
}

void fsm_outputs(const fsm_ctx_t *ctx, fsm_outputs_t *out)
{
    const uint8_t blink_on = (uint8_t)(((ctx->elapsed_ms / FSM_BLINK_HALF_MS) % 2u) == 0u);

    out->car_r = 0; out->car_y = 0; out->car_g = 0;
    out->ped_r = 0; out->ped_g = 0;

    switch (ctx->state) {
    case ST_INIT:
    case ST_RECOVERY:
        out->car_r = 1; out->ped_r = 1;              /* todos vermelhos        */
        break;
    case ST_VERDE_VEICULO:
    case ST_PEDIDO_PEDESTRE:
        out->car_g = 1; out->ped_r = 1;
        break;
    case ST_ATENCAO_VEICULO:
        out->car_y = 1; out->ped_r = 1;
        break;
    case ST_VERMELHO_VEICULO_VERDE_PEDESTRE:
        out->car_r = 1; out->ped_g = 1;
        break;
    case ST_ATENCAO_PEDESTRE:
        out->car_r = 1; out->ped_r = blink_on;       /* vermelho piscante ped. */
        break;
    case ST_FALHA:
        out->car_y = blink_on;                       /* amarelo piscante       */
        break;
    default:
        out->car_y = blink_on;                       /* idem ao estado seguro  */
        break;
    }
}

const char *fsm_state_name(uint8_t state)
{
    switch (state) {
    case ST_INIT:                            return "INIT";
    case ST_VERDE_VEICULO:                   return "VERDE_VEICULO";
    case ST_PEDIDO_PEDESTRE:                 return "PEDIDO_PEDESTRE";
    case ST_ATENCAO_VEICULO:                 return "ATENCAO_VEICULO";
    case ST_VERMELHO_VEICULO_VERDE_PEDESTRE: return "VERMELHO_VEICULO_VERDE_PEDESTRE";
    case ST_ATENCAO_PEDESTRE:                return "ATENCAO_PEDESTRE";
    case ST_FALHA:                           return "FALHA";
    case ST_RECOVERY:                        return "RECOVERY";
    default:                                 return "INVALIDO";
    }
}
