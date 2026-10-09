/*
 * sim_main.c - Simulador de host do semaforo (Plano B da PTT7)
 *
 * Executa o MESMO core/fsm_core.c do firmware no computador, com entradas
 * controladas e atuador emulado no terminal. Tempos medidos aqui NAO sao
 * atribuidos ao ESP32 (ver protocolo, secao 9 - Plano B).
 *
 * Modos:
 *   ptt7_sim                         interativo, tempo real (passo de 10 ms)
 *   ptt7_sim --cenario arq.txt       roteiro em tempo virtual com verificacoes (teste de aceitacao)
 *   ptt7_sim --falhas N [--semente S] experimento de injecao de falhas em tempo virtual
 *                                    (gera linhas FAULT no mesmo formato CSV do firmware)
 *
 * Teclas no modo interativo:
 *   1 / 2  botao de pedestre 1 / 2         a / b  alterna perda de sinal do botao 1 / 2
 *   t      falha de temporizacao (trava o laco por DT_MAX+100 ms)
 *   r      reset                           s      status            q  sair
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fsm_core.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
static uint64_t now_ms(void) { return (uint64_t)GetTickCount64(); }
static void sleep_ms(unsigned ms) { Sleep(ms); }
static int key_get(void) { return _kbhit() ? _getch() : -1; }
static void term_raw(int on) { (void)on; }
#else
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
static void sleep_ms(unsigned ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
static struct termios g_old;
static void term_raw(int on)
{
    if (!isatty(0)) return;
    if (on) {
        struct termios t;
        tcgetattr(0, &g_old);
        t = g_old;
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        tcsetattr(0, TCSANOW, &t);
    } else {
        tcsetattr(0, TCSANOW, &g_old);
    }
}
static int key_get(void)
{
    fd_set fds;
    struct timeval tv = { 0, 0 };
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    if (select(1, &fds, NULL, NULL, &tv) > 0) {
        unsigned char c;
        if (read(0, &c, 1) == 1) return c;
    }
    return -1;
}
#endif

#define STEP_MS 10u

static const char *lamp(uint8_t on, const char *cor) { return on ? cor : "--"; }

static void print_panel(const fsm_ctx_t *c, uint64_t t_ms, uint8_t tr)
{
    fsm_outputs_t o;
    fsm_outputs(c, &o);
    printf("[%8.3f s] %-34s VEIC[%s %s %s]  PED[%s %s]",
           (double)t_ms / 1000.0, fsm_state_name(c->state),
           lamp(o.car_r, "VM"), lamp(o.car_y, "AM"), lamp(o.car_g, "VD"),
           lamp(o.ped_r, "VM"), lamp(o.ped_g, "VD"));
    if (tr != TR_NONE) printf("  <- T%u", tr == TR_DEFENSIVE ? 99u : tr);
    printf("\n");
    fflush(stdout);
}

/* ------------------------------------------------------------ interativo */
static int run_interactive(void)
{
    fsm_ctx_t c;
    fsm_inputs_t in;
    int loss1 = 0, loss2 = 0, stall = 0;
    fsm_init(&c);
    term_raw(1);
    printf("Simulador PTT7 (Plano B) - teclas: 1/2 pedestre, a/b perda de sinal, t falha temporal, r reset, s status, q sair\n");
    uint64_t t0 = now_ms(), last = t0;
    uint8_t last_blink[2] = { 2, 2 };
    print_panel(&c, 0, TR_NONE);
    for (;;) {
        memset(&in, 0, sizeof in);
        int k;
        while ((k = key_get()) != -1) {
            switch (k) {
            case '1': in.req1 = 1; printf("  > botao pedestre 1\n"); break;
            case '2': in.req2 = 1; printf("  > botao pedestre 2\n"); break;
            case 'a': loss1 = !loss1; printf("  > sinal botao 1: %s\n", loss1 ? "PERDIDO" : "restaurado"); break;
            case 'b': loss2 = !loss2; printf("  > sinal botao 2: %s\n", loss2 ? "PERDIDO" : "restaurado"); break;
            case 't': stall = 1; printf("  > falha de temporizacao injetada (%u ms)\n", FSM_DT_MAX_MS + 100u); break;
            case 'r': in.reset = 1; printf("  > reset\n"); break;
            case 's': print_panel(&c, now_ms() - t0, TR_NONE); break;
            case 'q': term_raw(0); return 0;
            default: break;
            }
        }
        if (stall) { sleep_ms(FSM_DT_MAX_MS + 100u); stall = 0; }
        uint64_t now = now_ms();
        in.dt_ms = (uint32_t)(now - last);
        last = now;
        in.sig1_ok = !loss1;
        in.sig2_ok = !loss2;
        uint8_t tr = fsm_step(&c, &in);
        fsm_outputs_t o;
        fsm_outputs(&c, &o);
        uint8_t blink[2] = { o.car_y, o.ped_r };
        int blink_changed = (c.state == ST_FALHA || c.state == ST_ATENCAO_PEDESTRE) &&
                            (blink[0] != last_blink[0] || blink[1] != last_blink[1]);
        if (tr != TR_NONE || blink_changed) print_panel(&c, now - t0, tr);
        last_blink[0] = blink[0];
        last_blink[1] = blink[1];
        sleep_ms(STEP_MS);
    }
}

/* ---------------------------------------------------------------- cenario */
static int state_from_name(const char *n)
{
    for (int s = 0; s < FSM_NUM_STATES; s++)
        if (strcmp(fsm_state_name((uint8_t)s), n) == 0) return s;
    return -1;
}

/*
 * Formato do roteiro (uma acao por linha, '#' = comentario):
 *   <t_ms> req1 | req2 | reset | perda1 | perda2 | perda12 | restaura1 | restaura2 | restaura12 | atraso <ms>
 *   <t_ms> espera <ESTADO>          verifica o estado no instante t
 *   <t_ms> verde_off                verifica que nenhum verde esta aceso
 *   <t_ms> fim
 */
static int run_scenario(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { printf("nao foi possivel abrir %s\n", path); return 2; }
    fsm_ctx_t c;
    fsm_init(&c);
    uint64_t t = 0;
    int loss1 = 0, loss2 = 0, pass = 0, fail = 0;
    char line[256];
    printf("== Cenario %s ==\n", path);
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#');
        if (h) *h = 0;
        unsigned long at;
        char cmd[64] = "", arg[64] = "";
        if (sscanf(line, "%lu %63s %63s", &at, cmd, arg) < 2) continue;
        /* avanca o tempo virtual ate 'at' em passos de 10 ms */
        while (t + STEP_MS <= at) {
            fsm_inputs_t in;
            memset(&in, 0, sizeof in);
            in.sig1_ok = !loss1; in.sig2_ok = !loss2; in.dt_ms = STEP_MS;
            uint8_t tr = fsm_step(&c, &in);
            t += STEP_MS;
            if (tr != TR_NONE) print_panel(&c, t, tr);
        }
        fsm_inputs_t ev;
        memset(&ev, 0, sizeof ev);
        ev.sig1_ok = !loss1; ev.sig2_ok = !loss2;
        int step_now = 0;
        if (!strcmp(cmd, "req1")) { ev.req1 = 1; step_now = 1; }
        else if (!strcmp(cmd, "req2")) { ev.req2 = 1; step_now = 1; }
        else if (!strcmp(cmd, "reset")) { ev.reset = 1; step_now = 1; }
        else if (!strcmp(cmd, "perda1")) { loss1 = 1; ev.sig1_ok = 0; step_now = 1; }
        else if (!strcmp(cmd, "perda2")) { loss2 = 1; ev.sig2_ok = 0; step_now = 1; }
        else if (!strcmp(cmd, "perda12")) { loss1 = loss2 = 1; ev.sig1_ok = ev.sig2_ok = 0; step_now = 1; }
        else if (!strcmp(cmd, "restaura12")) { loss1 = loss2 = 0; ev.sig1_ok = ev.sig2_ok = 1; step_now = 1; }
        else if (!strcmp(cmd, "restaura1")) { loss1 = 0; ev.sig1_ok = 1; step_now = 1; }
        else if (!strcmp(cmd, "restaura2")) { loss2 = 0; ev.sig2_ok = 1; step_now = 1; }
        else if (!strcmp(cmd, "atraso")) { ev.dt_ms = (uint32_t)strtoul(arg, NULL, 10); t += ev.dt_ms; step_now = 1; }
        else if (!strcmp(cmd, "espera")) {
            int want = state_from_name(arg);
            int ok = (want == c.state);
            printf("  [%s] t=%lu ms espera %s (atual %s)\n", ok ? "PASSOU" : "FALHOU", at, arg, fsm_state_name(c.state));
            ok ? pass++ : fail++;
        } else if (!strcmp(cmd, "verde_off")) {
            fsm_outputs_t o;
            fsm_outputs(&c, &o);
            int ok = !o.car_g && !o.ped_g;
            printf("  [%s] t=%lu ms todos os verdes apagados\n", ok ? "PASSOU" : "FALHOU", at);
            ok ? pass++ : fail++;
        } else if (!strcmp(cmd, "fim")) break;
        else printf("  (comando desconhecido ignorado: %s)\n", cmd);
        if (step_now) {
            printf("  > t=%lu ms %s %s\n", at, cmd, arg);
            uint8_t tr = fsm_step(&c, &ev);
            if (tr != TR_NONE) print_panel(&c, t, tr);
        }
    }
    fclose(f);
    printf("Cenario: %d verificacoes passaram, %d falharam -> %s\n", pass, fail, fail ? "FALHOU" : "OK");
    return fail ? 1 : 0;
}

/* --------------------------------------------- experimento de falhas (virtual) */
static uint32_t g_rng = 2026;
static uint32_t rnd(void) { g_rng = g_rng * 1103515245u + 12345u; return (g_rng >> 8) & 0xFFFFFF; }

static void advance(fsm_ctx_t *c, uint64_t *t, uint32_t ms, int l1, int l2)
{
    fsm_inputs_t in;
    while (ms >= STEP_MS) {
        memset(&in, 0, sizeof in);
        in.sig1_ok = !l1; in.sig2_ok = !l2; in.dt_ms = STEP_MS;
        fsm_step(c, &in);
        *t += STEP_MS;
        ms -= STEP_MS;
    }
}

static int run_fault_experiment(int n)
{
    printf("# PTT7 simulador (Plano B) - tempo VIRTUAL, passo %u ms - tempos NAO atribuiveis ao ESP32\n", STEP_MS);
    printf("# FAULT,sessao,seq,tipo,estado_injecao,t_inj_us,conv_ms,sucesso,verde_apos_falha,plataforma\n");
    int total_ok = 0, total = 0;
    for (int tipo = 1; tipo <= 4; tipo++) {
        for (int k = 0; k < n; k++) {
            fsm_ctx_t c;
            fsm_init(&c);
            uint64_t t = 0;
            advance(&c, &t, FSM_T_INIT_MS, 0, 0);                       /* -> VV */
            if (tipo == 3) {                                           /* leva a VP */
                advance(&c, &t, FSM_T_VERDE_MIN_MS, 0, 0);
                fsm_inputs_t r; memset(&r, 0, sizeof r); r.sig1_ok = r.sig2_ok = 1; r.req1 = 1;
                fsm_step(&c, &r);
                advance(&c, &t, FSM_T_AMARELO_MS + STEP_MS, 0, 0);
                advance(&c, &t, (rnd() % (FSM_T_VERDE_PED_MS - 500)) / STEP_MS * STEP_MS, 0, 0);
            } else {
                advance(&c, &t, (rnd() % 5000) / STEP_MS * STEP_MS, 0, 0);
            }
            uint8_t st_inj = c.state;
            /* injecao em instante aleatorio dentro do passo */
            uint32_t phase_us = rnd() % (STEP_MS * 1000u);
            uint64_t t_inj_us = t * 1000u + phase_us;
            int l1 = (tipo == 1 || tipo == 3), l2 = (tipo == 2 || tipo == 3);
            fsm_inputs_t in; memset(&in, 0, sizeof in);
            in.sig1_ok = !l1; in.sig2_ok = !l2;
            in.dt_ms = (tipo == 4) ? FSM_DT_MAX_MS + 100u : STEP_MS;
            fsm_step(&c, &in);
            t += in.dt_ms;
            double conv_ms = (double)(t * 1000u - t_inj_us) / 1000.0;
            int reached = (c.state == ST_FALHA);
            /* janela de observacao de 1 s */
            int green = 0;
            for (uint32_t w = 0; w < 1000; w += STEP_MS) {
                advance(&c, &t, STEP_MS, (tipo == 4) ? 0 : l1, (tipo == 4) ? 0 : l2);
                fsm_outputs_t o; fsm_outputs(&c, &o);
                if (o.car_g || o.ped_g) green = 1;
            }
            int ok = reached && conv_ms <= 1000.0 && !green && c.state == ST_FALHA;
            printf("FAULT,0,%d,%d,%s,%llu,%.3f,%d,%d,sim\n", k, tipo, fsm_state_name(st_inj),
                   (unsigned long long)t_inj_us, conv_ms, ok, green);
            total++; total_ok += ok;
        }
    }
    printf("# sucesso: %d/%d\n", total_ok, total);
    return total_ok == total ? 0 : 1;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--semente") && i + 1 < argc) g_rng = (uint32_t)strtoul(argv[i + 1], NULL, 10);
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--cenario") && i + 1 < argc) return run_scenario(argv[i + 1]);
        if (!strcmp(argv[i], "--falhas") && i + 1 < argc) return run_fault_experiment(atoi(argv[i + 1]));
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("uso: %s [--cenario arquivo.txt] [--falhas N [--semente S]]\n", argv[0]);
            return 0;
        }
    }
    return run_interactive();
}
