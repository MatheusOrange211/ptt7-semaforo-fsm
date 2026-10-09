/*
 * harness_reach.c - P1 (alcancabilidade) sobre o codigo C (ESBMC), de forma composicional
 *
 * Consulta existencial codificada como assercao negada: afirmamos que o estado
 * destino nunca e atingido em K passos. Um CONTRAEXEMPLO do ESBMC (VERIFICATION
 * FAILED) e, portanto, um CAMINHO => trecho ALCANCADO. VERIFICATION SUCCESSFUL
 * significa "nao alcancado ate K" (inconclusivo).
 *
 * Por que composicional: com DT_MAX = 200 ms o ciclo completo exige ~130 passos e
 * o BMC monolitico a partir de INIT nao termina em tempo razoavel. Cada SEGMENTO
 * parte do contexto EXATO que fsm_step() produz ao entrar no estado de origem
 * (elapsed = 0, green e pending conforme a transicao de entrada); como a cadeia
 * de segmentos comeca em fsm_init(), a composicao prova a alcancabilidade a partir
 * de INIT. Nos segmentos 4-6 o valor de green_ms nao e lido por nenhuma guarda
 * (so VV/PP o usam), portanto o valor escolhido nao afeta o resultado. dt e fixado em DT_MAX (consulta existencial: restringir entradas nao
 * invalida o caminho encontrado); booleanos permanecem nao deterministicos.
 *
 * Segmentos (-DSEG=n):
 *   1 INIT(init)                          -> VERDE_VEICULO       K = 11
 *   2 VERDE_VEICULO(gr=0,pend=0)          -> PEDIDO_PEDESTRE     K = 2
 *   3 PEDIDO_PEDESTRE(gr=DT,pend=1)       -> ATENCAO_VEICULO     K = 41
 *   4 ATENCAO_VEICULO(pend=1)             -> VERM_VEIC_VERDE_PED K = 16
 *   5 VERM_VEIC_VERDE_PED(pend=0)         -> ATENCAO_PEDESTRE    K = 31
 *   6 ATENCAO_PEDESTRE(pend=0)            -> VERDE_VEICULO       K = 16
 *   7 INIT(init)                          -> FALHA               K = 2  (falha permitida)
 *   8 FALHA(pend=0)                       -> RECOVERY            K = 2
 *
 * esbmc harness_reach.c ../../core/fsm_core.c -I../../core --z3 -DSEG=3 --unwind 42 --no-unwinding-assertions
 */
#include "harness_common.h"

#ifndef SEG
#define SEG 1
#endif

#if SEG == 1
#  define FROM ST_INIT
#  define TO   ST_VERDE_VEICULO
#  define K    11
#  define GR   0
#  define PEND 0
#elif SEG == 2
#  define FROM ST_VERDE_VEICULO
#  define TO   ST_PEDIDO_PEDESTRE
#  define K    2
#  define GR   0
#  define PEND 0
#elif SEG == 3
#  define FROM ST_PEDIDO_PEDESTRE
#  define TO   ST_ATENCAO_VEICULO
#  define K    41
#  define GR   FSM_DT_MAX_MS
#  define PEND 1
#elif SEG == 4
#  define FROM ST_ATENCAO_VEICULO
#  define TO   ST_VERMELHO_VEICULO_VERDE_PEDESTRE
#  define K    16
#  define GR   (FSM_T_VERDE_MIN_MS)
#  define PEND 1
#elif SEG == 5
#  define FROM ST_VERMELHO_VEICULO_VERDE_PEDESTRE
#  define TO   ST_ATENCAO_PEDESTRE
#  define K    31
#  define GR   (FSM_T_VERDE_MIN_MS)
#  define PEND 0
#elif SEG == 6
#  define FROM ST_ATENCAO_PEDESTRE
#  define TO   ST_VERDE_VEICULO
#  define K    16
#  define GR   (FSM_T_VERDE_MIN_MS)
#  define PEND 0
#elif SEG == 7
#  define FROM ST_INIT
#  define TO   ST_FALHA
#  define K    2
#  define GR   0
#  define PEND 0
#elif SEG == 8
#  define FROM ST_FALHA
#  define TO   ST_RECOVERY
#  define K    2
#  define GR   0
#  define PEND 0
#else
#  error "SEG invalido (1..8)"
#endif

int main(void)
{
    fsm_ctx_t c;
    fsm_init(&c);                 /* contexto de entrada do estado de origem */
    c.state = FROM;
    c.elapsed_ms = 0;
    c.green_ms = GR;
    c.pending = PEND;
    const int allow_fault = (TO == ST_FALHA);

    for (int j = 0; j < K; j++) {
        fsm_inputs_t in = nd_inputs();
        if (allow_fault) {
            __ESBMC_assume(in.dt_ms <= FSM_DT_MAX_MS + 1);
        } else {
            __ESBMC_assume(in.dt_ms == FSM_DT_MAX_MS);
            __ESBMC_assume(in.sig1_ok && in.sig2_ok);
        }
        fsm_step(&c, &in);
        assert(c.state != TO);    /* violacao = caminho encontrado */
    }
    return 0;
}
