/*
 * test_fsm_core.c - Testes unitarios do nucleo (host, sem hardware)
 * Cobre as 16 transicoes, a funcao de saida e as propriedades P3, P3b, P4, P6.
 *
 * Compilar/rodar:  python tools/ptt7.py test     (ou manualmente:)
 *   gcc -std=c99 -Wall -Wextra -Icore core/fsm_core.c tests/test_fsm_core.c -o build/test_fsm_core
 */
#include <stdio.h>
#include <string.h>
#include "fsm_core.h"

static int g_run = 0, g_fail = 0;

#define CHECK(cond, msg) do { g_run++; if (!(cond)) { g_fail++; \
    printf("  [FALHOU] %s:%d  %s\n", __FILE__, __LINE__, msg); } } while (0)

static fsm_inputs_t in_ok(uint32_t dt)
{
    fsm_inputs_t in;
    memset(&in, 0, sizeof in);
    in.sig1_ok = 1; in.sig2_ok = 1; in.dt_ms = dt;
    return in;
}

/* avanca o tempo em passos de 100 ms sem eventos */
static void run_ms(fsm_ctx_t *c, uint32_t ms)
{
    fsm_inputs_t in = in_ok(100);
    while (ms >= 100) { fsm_step(c, &in); ms -= 100; }
    if (ms) { in.dt_ms = ms; fsm_step(c, &in); }
}

static void press(fsm_ctx_t *c, int which)
{
    fsm_inputs_t in = in_ok(10);
    if (which == 1) in.req1 = 1; else in.req2 = 1;
    fsm_step(c, &in);
}

static void go_to_vv(fsm_ctx_t *c) { fsm_init(c); run_ms(c, FSM_T_INIT_MS); }

static void test_init(void)
{
    fsm_ctx_t c; fsm_init(&c);
    CHECK(c.state == ST_INIT, "estado inicial INIT");
    run_ms(&c, FSM_T_INIT_MS - 100);
    CHECK(c.state == ST_INIT, "INIT permanece antes de T_INIT");
    run_ms(&c, 100);
    CHECK(c.state == ST_VERDE_VEICULO && c.last_tr == TR_T1, "T1 INIT->VV");

    fsm_init(&c);
    fsm_inputs_t in = in_ok(10); in.sig1_ok = 0;
    CHECK(fsm_step(&c, &in) == TR_T2 && c.state == ST_FALHA, "T2 INIT->FALHA");
}

static void test_ciclo_normal(void)
{
    fsm_ctx_t c; go_to_vv(&c);
    run_ms(&c, 30000);
    CHECK(c.state == ST_VERDE_VEICULO, "sem demanda permanece em VV");

    press(&c, 1);
    CHECK(c.state == ST_ATENCAO_VEICULO && c.last_tr == TR_T4, "T4 VV->AV (verde minimo cumprido)");
    run_ms(&c, FSM_T_AMARELO_MS);
    CHECK(c.state == ST_VERMELHO_VEICULO_VERDE_PEDESTRE && c.last_tr == TR_T8, "T8 AV->VP");
    run_ms(&c, FSM_T_VERDE_PED_MS);
    CHECK(c.state == ST_ATENCAO_PEDESTRE && c.last_tr == TR_T10, "T10 VP->AP");
    run_ms(&c, FSM_T_ATENCAO_PED_MS);
    CHECK(c.state == ST_VERDE_VEICULO && c.last_tr == TR_T12, "T12 AP->VV");
    CHECK(c.green_ms == 0, "green zerado ao reentrar em VV");
}

static void test_verde_minimo(void)
{
    fsm_ctx_t c; go_to_vv(&c);
    run_ms(&c, 1000);
    press(&c, 2);
    CHECK(c.state == ST_PEDIDO_PEDESTRE && c.last_tr == TR_T3, "T3 VV->PP (verde < minimo)");
    run_ms(&c, FSM_T_VERDE_MIN_MS - 1000 - 10 - 100);
    CHECK(c.state == ST_PEDIDO_PEDESTRE, "P3b: PP espera completar verde minimo");
    run_ms(&c, 100);
    CHECK(c.state == ST_ATENCAO_VEICULO && c.last_tr == TR_T6, "T6 PP->AV");
}

static void test_latch_demanda(void)
{
    fsm_ctx_t c; go_to_vv(&c);
    run_ms(&c, FSM_T_VERDE_MIN_MS);
    press(&c, 1);                              /* -> AV */
    run_ms(&c, FSM_T_AMARELO_MS);              /* -> VP */
    press(&c, 2);                              /* pedido durante VP: ignorado */
    CHECK(c.pending == 0, "pedido durante VP nao e memorizado");
    run_ms(&c, FSM_T_VERDE_PED_MS);            /* -> AP */
    press(&c, 1);                              /* pedido durante AP: memorizado */
    CHECK(c.pending == 1, "pedido durante AP e memorizado");
    run_ms(&c, FSM_T_ATENCAO_PED_MS);          /* -> VV, no proximo passo -> PP */
    run_ms(&c, 100);
    CHECK(c.state == ST_PEDIDO_PEDESTRE, "demanda memorizada atendida no ciclo seguinte");
}

static void test_falhas_em_todos_estados(void)
{
    /* P4: a partir de cada estado alcancavel, uma entrada com falha leva a FALHA */
    static const struct { uint8_t st; uint8_t tr; } casos[] = {
        { ST_INIT, TR_T2 }, { ST_VERDE_VEICULO, TR_T5 }, { ST_PEDIDO_PEDESTRE, TR_T7 },
        { ST_ATENCAO_VEICULO, TR_T9 }, { ST_VERMELHO_VEICULO_VERDE_PEDESTRE, TR_T11 },
        { ST_ATENCAO_PEDESTRE, TR_T13 }, { ST_RECOVERY, TR_T16 },
    };
    for (unsigned i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        for (int tipo = 0; tipo < 3; tipo++) {
            fsm_ctx_t c; fsm_init(&c);
            c.state = casos[i].st; c.elapsed_ms = 50; c.green_ms = 50;
            fsm_inputs_t in = in_ok(10);
            if (tipo == 0) in.sig1_ok = 0;
            if (tipo == 1) in.sig2_ok = 0;
            if (tipo == 2) in.dt_ms = FSM_DT_MAX_MS + 1;   /* falha de temporizacao */
            uint8_t tr = fsm_step(&c, &in);
            char msg[96];
            snprintf(msg, sizeof msg, "P4 %s tipo %d -> FALHA", fsm_state_name(casos[i].st), tipo);
            CHECK(c.state == ST_FALHA && tr == casos[i].tr, msg);
        }
    }
    /* limite: dt == DT_MAX nao e falha */
    fsm_ctx_t c; go_to_vv(&c);
    fsm_inputs_t in = in_ok(FSM_DT_MAX_MS);
    fsm_step(&c, &in);
    CHECK(c.state == ST_VERDE_VEICULO, "dt == DT_MAX ainda e aceito");
}

static void test_falha_e_recovery(void)
{
    fsm_ctx_t c; go_to_vv(&c);
    fsm_inputs_t in = in_ok(10); in.sig1_ok = 0; in.sig2_ok = 0;
    fsm_step(&c, &in);
    CHECK(c.state == ST_FALHA, "perda simultanea -> FALHA");

    in.reset = 1;                                   /* reset com falha ainda presente */
    fsm_step(&c, &in);
    CHECK(c.state == ST_FALHA, "reset ignorado enquanto falha persiste (P5)");

    in = in_ok(10); in.req1 = 1;
    fsm_step(&c, &in);
    CHECK(c.state == ST_FALHA && c.pending == 0, "FALHA sem reset permanece e nao memoriza pedido");

    in = in_ok(10); in.reset = 1;
    uint8_t tr = fsm_step(&c, &in);
    CHECK(c.state == ST_RECOVERY && tr == TR_T14, "T14 FALHA->RECOVERY (P6)");
    run_ms(&c, FSM_T_RECOVERY_MS);
    CHECK(c.state == ST_VERDE_VEICULO && c.last_tr == TR_T15, "T15 RECOVERY->VV");
}

static void test_saidas(void)
{
    fsm_ctx_t c; fsm_outputs_t o;
    for (uint8_t s = 0; s < FSM_NUM_STATES; s++) {
        for (uint32_t e = 0; e < 3000; e += 250) {
            fsm_init(&c); c.state = s; c.elapsed_ms = e;
            fsm_outputs(&c, &o);
            CHECK(!(o.ped_g && (o.car_g || o.car_y)), "P3 exclusao mutua de verdes");
            CHECK(!o.ped_g || s == ST_VERMELHO_VEICULO_VERDE_PEDESTRE, "P3 verde pedestre so em VP");
            CHECK((o.car_r + o.car_y + o.car_g) <= 1, "no maximo uma luz veicular");
        }
    }
    fsm_init(&c); c.state = ST_FALHA; c.elapsed_ms = 0;   fsm_outputs(&c, &o);
    CHECK(o.car_y == 1 && o.car_g == 0 && o.ped_g == 0, "FALHA: amarelo aceso na 1a meia-onda");
    c.elapsed_ms = FSM_BLINK_HALF_MS;                      fsm_outputs(&c, &o);
    CHECK(o.car_y == 0, "FALHA: amarelo apagado na 2a meia-onda (pisca)");
}

static void test_defensivo_e_saturacao(void)
{
    fsm_ctx_t c; fsm_init(&c);
    c.state = 42;
    fsm_inputs_t in = in_ok(10);
    CHECK(fsm_step(&c, &in) == TR_DEFENSIVE && c.state == ST_FALHA, "estado invalido -> FALHA");

    go_to_vv(&c);
    in = in_ok(FSM_DT_MAX_MS);
    for (int i = 0; i < 1000; i++) fsm_step(&c, &in);
    CHECK(c.elapsed_ms == FSM_TIMER_SAT_MS && c.green_ms == FSM_TIMER_SAT_MS, "temporizadores saturam");
    CHECK(fsm_ctx_valid(&c), "contexto permanece valido");
}

int main(void)
{
    printf("== Testes unitarios do nucleo fsm_core ==\n");
    test_init();
    test_ciclo_normal();
    test_verde_minimo();
    test_latch_demanda();
    test_falhas_em_todos_estados();
    test_falha_e_recovery();
    test_saidas();
    test_defensivo_e_saturacao();
    printf("Resultado: %d verificacoes, %d falhas\n", g_run, g_fail);
    printf(g_fail ? "STATUS: FALHOU\n" : "STATUS: OK\n");
    return g_fail ? 1 : 0;
}
