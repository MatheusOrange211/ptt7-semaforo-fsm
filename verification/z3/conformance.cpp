// conformance.cpp - Teste de conformidade modelo (C++) x codigo (core/fsm_core.c)
//
// Mitigacao da principal ameaca a validade (divergencia modelo x codigo): os MESMOS
// vetores de entrada sao aplicados ao nucleo em C (o mesmo do firmware) e a
// instanciacao concreta do modelo usado no Z3 (fsm_model.hpp com ConcOps). Apos cada
// passo comparam-se estado, transicao, temporizadores, latch e as 5 saidas.
//
// Uso: conformance [--seqs 400] [--len 250] [--seed 2026] [--vectors arquivo.csv]
// Saida: codigo 0 se nao houver divergencia; cobertura de estados e transicoes T1..T16.
#include "fsm_model.hpp"

extern "C" {
#include "../../core/fsm_core.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

using namespace fsm;

int main(int argc, char **argv)
{
    int seqs = 400, len = 250;
    unsigned seed = 2026;
    std::string vec_path = "verification/results/conformance_vectors.csv";
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--seqs" && i + 1 < argc) seqs = std::atoi(argv[++i]);
        else if (a == "--len" && i + 1 < argc) len = std::atoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = unsigned(std::strtoul(argv[++i], nullptr, 10));
        else if (a == "--vectors" && i + 1 < argc) vec_path = argv[++i];
        else { std::printf("uso: %s [--seqs N] [--len L] [--seed S] [--vectors arq.csv]\n", argv[0]); return 2; }
    }

    std::mt19937 rng(seed);
    auto permil = [&](int p) { return int(rng() % 1000) < p; };   // probabilidade em por mil
    FILE *vf = std::fopen(vec_path.c_str(), "w");
    if (vf) std::fprintf(vf, "seq,passo,req1,req2,sig1_ok,sig2_ok,reset,dt_ms,estado_c,tr_c\n");

    ConcOps o;
    long divergencias = 0, passos = 0;
    int cov_state[NUM_STATES] = {0};
    int cov_tr[17] = {0};

    for (int q = 0; q < seqs; q++) {
        fsm_ctx_t cc;
        fsm_init(&cc);
        Ctx<ConcOps> mc{INIT, 0, 0, false};
        // perfil de cada sequencia: algumas sem falhas (para percorrer o ciclo completo)
        const bool sem_falha = (q % 3) != 0;
        const int p_req = (q % 2) ? 80 : 3;   // demanda frequente (T3) ou rara (T4: verde minimo ja cumprido)

        for (int j = 0; j < len; j++) {
            fsm_inputs_t in;
            std::memset(&in, 0, sizeof in);
            in.req1 = permil(p_req);
            in.req2 = permil(p_req);
            in.sig1_ok = sem_falha ? 1 : !permil(5);
            in.sig2_ok = sem_falha ? 1 : !permil(5);
            in.reset = permil(150);
            int r = int(rng() % 1000);
            if (r < 700 || (sem_falha && r < 990)) in.dt_ms = FSM_DT_MAX_MS;
            else if (r < 990 || sem_falha) in.dt_ms = rng() % (FSM_DT_MAX_MS + 1);
            else if (r < 997) in.dt_ms = FSM_DT_MAX_MS + 1 + rng() % 1000;   // falha de temporizacao
            else in.dt_ms = 0xFFFFFFF0u + (rng() % 16);                     // atraso extremo (saturacao)

            Inp<ConcOps> mi{in.req1 != 0, in.req2 != 0, in.sig1_ok != 0, in.sig2_ok != 0, in.reset != 0,
                            (long long)in.dt_ms};
            long long mtr = 0;
            Ctx<ConcOps> mn = model_step<ConcOps>(o, mc, mi, false, nullptr, &mtr);
            uint8_t ctr = fsm_step(&cc, &in);

            fsm_outputs_t co;
            fsm_outputs(&cc, &co);
            Out<ConcOps> mo = model_outputs(o, mn);

            bool ok = mn.s == cc.state && mtr == ctr && mn.el == cc.elapsed_ms && mn.gr == cc.green_ms &&
                      mn.pend == (cc.pending != 0) && mo.car_r == (co.car_r != 0) && mo.car_y == (co.car_y != 0) &&
                      mo.car_g == (co.car_g != 0) && mo.ped_r == (co.ped_r != 0) && mo.ped_g == (co.ped_g != 0);
            if (!ok) {
                if (divergencias < 10)
                    std::printf("DIVERGENCIA seq %d passo %d: C=(%s tr=%u el=%u gr=%u p=%u) modelo=(%s tr=%lld el=%lld gr=%lld p=%d)\n",
                                q, j, fsm_state_name(cc.state), ctr, cc.elapsed_ms, cc.green_ms, cc.pending,
                                state_name(mn.s), mtr, mn.el, mn.gr, int(mn.pend));
                divergencias++;
            }
            if (vf)
                std::fprintf(vf, "%d,%d,%u,%u,%u,%u,%u,%u,%s,%u\n", q, j, in.req1, in.req2, in.sig1_ok, in.sig2_ok,
                             in.reset, in.dt_ms, fsm_state_name(cc.state), ctr);
            cov_state[cc.state < NUM_STATES ? cc.state : 0]++;
            if (ctr >= 1 && ctr <= 16) cov_tr[ctr]++;
            mc = mn;
            passos++;
        }
    }
    if (vf) std::fclose(vf);

    std::printf("Conformidade modelo x codigo: %d sequencias x %d passos = %ld passos, semente %u\n", seqs, len, passos, seed);
    std::printf("Cobertura de estados: ");
    int st_cov = 0;
    for (int s = 0; s < NUM_STATES; s++) { std::printf("%s=%d ", STATE_NAMES[s], cov_state[s]); st_cov += cov_state[s] > 0; }
    std::printf("\nCobertura de transicoes: ");
    int tr_cov = 0;
    for (int t = 1; t <= 16; t++) { std::printf("T%d=%d ", t, cov_tr[t]); tr_cov += cov_tr[t] > 0; }
    std::printf("\nEstados cobertos: %d/8 | transicoes cobertas: %d/16 | divergencias: %ld\n", st_cov, tr_cov, divergencias);
    std::printf("Vetores gravados em %s\n", vec_path.c_str());
    std::printf(divergencias == 0 && tr_cov == 16 ? "STATUS: OK\n" : "STATUS: FALHOU\n");
    return (divergencias == 0 && tr_cov == 16) ? 0 : 1;
}
