/*
 * harness_common.h - utilitarios comuns aos harnesses ESBMC (camada 2: codigo C)
 * Os harnesses incluem o MESMO core/fsm_core.c usado no firmware.
 */
#ifndef HARNESS_COMMON_H
#define HARNESS_COMMON_H

#include <assert.h>
#include <stdint.h>
#include "fsm_core.h"

unsigned char __VERIFIER_nondet_uchar(void);
unsigned int  __VERIFIER_nondet_uint(void);
void __ESBMC_assume(_Bool);

static uint8_t nd_bool(void) { uint8_t b = __VERIFIER_nondet_uchar(); __ESBMC_assume(b <= 1); return b; }

/* Entrada admissivel: booleanos 0/1 e dt qualquer uint32 (inclui atrasos enormes). */
static fsm_inputs_t nd_inputs(void)
{
    fsm_inputs_t in;
    in.req1 = nd_bool(); in.req2 = nd_bool();
    in.sig1_ok = nd_bool(); in.sig2_ok = nd_bool();
    in.reset = nd_bool();
    in._pad[0] = in._pad[1] = in._pad[2] = 0;
    in.dt_ms = __VERIFIER_nondet_uint();
    return in;
}

/* Contexto arbitrario dentro do dominio valido (para provas de 1 passo). */
static fsm_ctx_t nd_ctx_valid(void)
{
    fsm_ctx_t c;
    c.state = __VERIFIER_nondet_uchar();
    c.pending = nd_bool();
    c.last_tr = __VERIFIER_nondet_uchar();
    c._pad = 0;
    c.elapsed_ms = __VERIFIER_nondet_uint();
    c.green_ms = __VERIFIER_nondet_uint();
    __ESBMC_assume(fsm_ctx_valid(&c));
    return c;
}

/* P3 avaliada sobre as saidas de um contexto */
static void check_p3(const fsm_ctx_t *c)
{
    fsm_outputs_t o;
    fsm_outputs(c, &o);
    assert(!(o.ped_g && (o.car_g || o.car_y)));                       /* P3: exclusao mutua */
    assert(!o.ped_g || c->state == ST_VERMELHO_VEICULO_VERDE_PEDESTRE); /* P3: verde ped. so em VP */
}

#endif
