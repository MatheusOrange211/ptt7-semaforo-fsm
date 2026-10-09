/*
 * harness_bmc.c - Verificacao limitada (BMC) a partir de INIT sobre o codigo C (ESBMC)
 *
 * Executa K passos com entradas arbitrarias a partir de fsm_init() e verifica,
 * em cada passo: P3 (saidas), P4/P5 (falha -> FALHA no passo seguinte e
 * permanencia enquanto a falha persistir) e P6 (FALHA so sai para RECOVERY).
 * O resultado vale ate o horizonte K (padrao 10, conforme o protocolo).
 *
 * esbmc harness_bmc.c ../../core/fsm_core.c -I../../core --z3 --unwind 11 --unwinding-assertions -DK=10
 * Prova ilimitada (k-inducao): esbmc ... --k-induction --max-k-step 20 -DUNBOUNDED
 */
#include "harness_common.h"

#ifndef K
#define K 10
#endif

int main(void)
{
    fsm_ctx_t c;
    fsm_init(&c);
    check_p3(&c);

#ifdef UNBOUNDED
    while (1) {
#else
    for (int j = 0; j < K; j++) {
#endif
        const uint8_t prev = c.state;
        fsm_inputs_t in = nd_inputs();
        const int fault = fsm_input_fault(&in);
        fsm_step(&c, &in);

        assert(fsm_ctx_valid(&c));                                  /* P2 (dominio) */
        check_p3(&c);                                               /* P3 */
        if (fault) assert(c.state == ST_FALHA);                     /* P4 / P5 */
        if (prev == ST_FALHA)                                       /* P6 */
            assert(c.state == ST_FALHA || c.state == ST_RECOVERY);
    }
    return 0;
}
