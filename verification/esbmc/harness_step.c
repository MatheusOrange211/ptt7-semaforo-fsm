/*
 * harness_step.c - Provas de 1 passo sobre o codigo C (ESBMC)
 *
 * Contexto ARBITRARIO valido + entrada ARBITRARIA admissivel -> um fsm_step().
 * Como o dominio valido e preservado (P2b, verificado aqui), cada assercao provada
 * vale para qualquer execucao (argumento indutivo). Selecione a propriedade com
 * -DPROP_P2 / -DPROP_P3 / -DPROP_P3B / -DPROP_P4 / -DPROP_P6 (ou nenhuma = todas).
 *
 * esbmc harness_step.c ../../core/fsm_core.c -I../../core --z3 --overflow-check --unsigned-overflow-check
 */
#include "harness_common.h"

#if !defined(PROP_P2) && !defined(PROP_P3) && !defined(PROP_P3B) && !defined(PROP_P4) && !defined(PROP_P6)
#define PROP_P2
#define PROP_P3
#define PROP_P3B
#define PROP_P4
#define PROP_P6
#endif

int main(void)
{
    fsm_ctx_t c = nd_ctx_valid();
    const fsm_ctx_t before = c;
    fsm_inputs_t in = nd_inputs();
    const int fault = fsm_input_fault(&in);

    uint8_t tr = fsm_step(&c, &in);

#ifdef PROP_P2
    /* P2: transicao definida para toda entrada admissivel; dominio preservado */
    assert(fsm_ctx_valid(&c));
    assert(tr == TR_NONE || (tr >= TR_T1 && tr <= TR_T16));   /* nunca defensiva a partir de estado valido */
    assert((tr == TR_NONE) == (c.state == before.state));
#endif
#ifdef PROP_P3
    check_p3(&before);
    check_p3(&c);
#endif
#ifdef PROP_P3B
    /* P3b: fim do verde veicular sem falha so apos T_VERDE_MIN (green ja acumulado neste passo) */
    if ((before.state == ST_VERDE_VEICULO || before.state == ST_PEDIDO_PEDESTRE) &&
        c.state == ST_ATENCAO_VEICULO) {
        uint32_t g = before.green_ms + in.dt_ms;
        if (g < before.green_ms || g > FSM_TIMER_SAT_MS) g = FSM_TIMER_SAT_MS;
        assert(g >= FSM_T_VERDE_MIN_MS);
    }
#endif
#ifdef PROP_P4
    /* P4: violacao das entradas conduz ao estado seguro */
    if (fault) {
        assert(c.state == ST_FALHA);
    }
#endif
#ifdef PROP_P6
    /* P6: de FALHA so se sai para RECOVERY */
    if (before.state == ST_FALHA) {
        assert(c.state == ST_FALHA || c.state == ST_RECOVERY);
    }
#endif
    return 0;
}
