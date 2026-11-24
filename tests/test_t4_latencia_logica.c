/*
 * test_t4_latencia_logica.c - Latencia LOGICA da transicao critica T4 (complemento da Q2)
 *
 * Pergunta: com o verde minimo ja cumprido, quantos passos da FSM separam o pedido de pedestre
 * (req1 ou req2) da entrada em ATENCAO_VEICULO (T4)?
 * Varre o instante do pedido de 0 a 20 s apos o verde minimo (passo de 10 ms), para os dois botoes.
 *
 * IMPORTANTE: mede passos da FSM, NAO tempo do ESP32. Nao substitui a medicao de LREC em bancada.
 *
 * Compilar e rodar (na raiz do repositorio):
 *   gcc -std=c99 -O2 -Wall -Wextra -Werror -Icore tests/test_t4_latencia_logica.c core/fsm_core.c -o t4 && ./t4
 */
#include <stdio.h>
#include <string.h>
#include "fsm_core.h"

int main(void)
{
    int pior = 0, casos = 0;
    for (int extra = 0; extra <= 20000; extra += 10) {
        fsm_ctx_t c;
        fsm_inputs_t in;
        fsm_init(&c);
        memset(&in, 0, sizeof in);
        in.sig1_ok = 1;
        in.sig2_ok = 1;
        in.dt_ms = 10;
        int guarda = 0;
        while (c.state != ST_VERDE_VEICULO && guarda++ < 100000) fsm_step(&c, &in);
        while (c.green_ms < FSM_T_VERDE_MIN_MS + (uint32_t)extra) fsm_step(&c, &in);
        for (int b = 0; b < 2; b++) {
            fsm_ctx_t d = c;
            fsm_inputs_t r = in;
            if (b) r.req2 = 1; else r.req1 = 1;
            int passos = 0;
            uint8_t tr;
            do {
                tr = fsm_step(&d, &r);
                passos++;
                r.req1 = r.req2 = 0;
            } while (d.state == ST_VERDE_VEICULO && passos < 1000);
            if (d.state != ST_ATENCAO_VEICULO || tr != TR_T4) {
                printf("FALHA: extra=%d botao=%d estado=%d transicao=%d\n", extra, b + 1, d.state, tr);
                return 1;
            }
            if (passos > pior) pior = passos;
            casos++;
        }
    }
    printf("%d casos: pior caso = %d passo(s) da FSM entre o pedido e ATENCAO_VEICULO (T4)\n", casos, pior);
    printf("STATUS: %s\n", pior == 1 ? "OK (T4 ocorre no proprio passo que recebe o pedido)" : "VERIFICAR");
    return pior == 1 ? 0 : 1;
}
