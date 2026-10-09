// fsm_model.hpp - Modelo abstrato da FSM do semaforo (PTT7) para o Z3 (API C++)
//
// A MESMA funcao `model_step<Ops>()` e instanciada de duas formas:
//   * Z3Ops   (z3::expr)          -> consultas de verificacao (verify_z3.cpp)
//   * ConcOps (long long / bool)  -> conformidade com o C (conformance.cpp)
//
// Temporizacoes: #include direto de core/fsm_config.h (fonte unica de verdade).
// O modelo espelha spec/transition_table.csv (T1..T16), e NAO reutiliza o codigo C:
// e uma segunda implementacao, escrita a partir da tabela, para revisao cruzada.
#pragma once

#include <vector>
#include <string>
#include "../../core/fsm_config.h"

namespace fsm {

enum : int { INIT = 0, VV, PP, AV, VP, AP, FALHA, RECOVERY, NUM_STATES };
static const char *const STATE_NAMES[] = {
    "INIT", "VERDE_VEICULO", "PEDIDO_PEDESTRE", "ATENCAO_VEICULO",
    "VERMELHO_VEICULO_VERDE_PEDESTRE", "ATENCAO_PEDESTRE", "FALHA", "RECOVERY"};

inline const char *state_name(long long s) {
    return (s >= 0 && s < NUM_STATES) ? STATE_NAMES[s] : "INVALIDO";
}

template <class O> struct Ctx {
    typename O::I s, el, gr;
    typename O::B pend;
};
template <class O> struct Inp {
    typename O::B req1, req2, sig1_ok, sig2_ok, reset;
    typename O::I dt;
};
template <class O> struct Row {           // linha da tabela de transicoes
    int orig;
    typename O::B guard;
    int dest;
    int id;
};
template <class O> struct Info {
    typename O::B fault;
    typename O::I el1, gr1;
    std::vector<Row<O>> rows;
};
template <class O> struct Out {
    typename O::B car_r, car_y, car_g, ped_r, ped_g;
};

// ----------------------------------------------------------- funcao de passo
template <class O>
Ctx<O> model_step(O &o, const Ctx<O> &c, const Inp<O> &in, bool mutante, Info<O> *info, typename O::I *tr_out)
{
    using I = typename O::I;
    using B = typename O::B;
    const long long SAT = FSM_TIMER_SAT_MS;

    B fault = (!in.sig1_ok) || (!in.sig2_ok) || (in.dt > o.i(FSM_DT_MAX_MS));
    B req = in.req1 || in.req2;

    // 1) temporizadores (soma saturada)
    I el1 = o.min(c.el + in.dt, o.i(SAT));
    B in_green = (c.s == o.i(VV)) || (c.s == o.i(PP));
    I gr1 = o.ite(in_green, o.min(c.gr + in.dt, o.i(SAT)), c.gr);
    // 2) latch de demanda
    B pend1 = c.pend || (req && (c.s != o.i(VP)) && (c.s != o.i(FALHA)));
    B demand = pend1;

    // 3) tabela T1..T16 (a ordem dentro de cada estado define prioridade: falha primeiro)
    std::vector<Row<O>> rows = {
        {INIT, fault, FALHA, 2},
        {INIT, !fault && el1 >= o.i(FSM_T_INIT_MS), VV, 1},
        {VV, fault, FALHA, 5},
        {VV, !fault && demand && gr1 >= o.i(FSM_T_VERDE_MIN_MS), AV, 4},
        {VV, !fault && demand && gr1 < o.i(FSM_T_VERDE_MIN_MS), PP, 3},
        {PP, fault, FALHA, 7},
        {PP, !fault && gr1 >= o.i(FSM_T_VERDE_MIN_MS), AV, 6},
        {AV, fault, FALHA, 9},
        {AV, !fault && el1 >= o.i(FSM_T_AMARELO_MS), VP, 8},
        {VP, fault, FALHA, 11},
        {VP, !fault && el1 >= o.i(FSM_T_VERDE_PED_MS), AP, 10},
        {AP, fault, FALHA, 13},
        {AP, !fault && el1 >= o.i(FSM_T_ATENCAO_PED_MS), VV, 12},
        {FALHA, in.reset && !fault, mutante ? VP : RECOVERY, 14},
        {RECOVERY, fault, FALHA, 16},
        {RECOVERY, !fault && el1 >= o.i(FSM_T_RECOVERY_MS), VV, 15},
    };
    I nxt = c.s;
    I tr = o.i(0);
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {   // primeira linha tem prioridade
        B fire = (c.s == o.i(it->orig)) && it->guard;
        nxt = o.ite(fire, o.i(it->dest), nxt);
        tr = o.ite(fire, o.i(it->id), tr);
    }
    B invalid = (c.s < o.i(0)) || (c.s >= o.i(NUM_STATES));      // defensivo
    nxt = o.ite(invalid, o.i(FALHA), nxt);
    tr = o.ite(invalid, o.i(99), tr);

    // 4) efeitos de entrada
    B changed = tr != o.i(0);
    Ctx<O> n{nxt,
             o.ite(changed, o.i(0), el1),
             o.ite(changed && (((nxt == o.i(VV)) && (c.s != o.i(VV))) || (nxt == o.i(FALHA))), o.i(0), gr1),
             o.bite(changed && ((nxt == o.i(VP)) || (nxt == o.i(FALHA))), o.b(false), pend1)};
    if (info) { info->fault = fault; info->el1 = el1; info->gr1 = gr1; info->rows = rows; }
    if (tr_out) *tr_out = tr;
    return n;
}

template <class O>
Out<O> model_outputs(O &o, const Ctx<O> &c)
{
    using B = typename O::B;
    B blink = o.mod(o.div(c.el, FSM_BLINK_HALF_MS), 2) == o.i(0);
    B inv = (c.s < o.i(0)) || (c.s >= o.i(NUM_STATES));
    Out<O> r{
        (c.s == o.i(INIT)) || (c.s == o.i(RECOVERY)) || (c.s == o.i(VP)) || (c.s == o.i(AP)),
        (c.s == o.i(AV)) || (((c.s == o.i(FALHA)) || inv) && blink),
        (c.s == o.i(VV)) || (c.s == o.i(PP)),
        (c.s == o.i(INIT)) || (c.s == o.i(RECOVERY)) || (c.s == o.i(VV)) || (c.s == o.i(PP)) ||
            (c.s == o.i(AV)) || ((c.s == o.i(AP)) && blink),
        c.s == o.i(VP)};
    return r;
}

// ------------------------------------------------------- operadores concretos
struct ConcOps {
    using I = long long;
    using B = bool;
    I i(long long v) { return v; }
    B b(bool v) { return v; }
    I ite(B c, I a, I b_) { return c ? a : b_; }
    B bite(B c, B a, B b_) { return c ? a : b_; }
    I min(I a, I b_) { return a < b_ ? a : b_; }
    I div(I a, long long d) { return a / d; }
    I mod(I a, long long d) { return a % d; }
};

} // namespace fsm

#ifdef FSM_MODEL_WITH_Z3
#include <z3++.h>
namespace fsm {
struct Z3Ops {
    using I = z3::expr;
    using B = z3::expr;
    z3::context &ctx;
    explicit Z3Ops(z3::context &c) : ctx(c) {}
    I i(long long v) { return ctx.int_val(static_cast<int64_t>(v)); }
    B b(bool v) { return ctx.bool_val(v); }
    I ite(const B &c, const I &a, const I &b_) { return z3::ite(c, a, b_); }
    B bite(const B &c, const B &a, const B &b_) { return z3::ite(c, a, b_); }
    I min(const I &a, const I &b_) { return z3::ite(a < b_, a, b_); }
    I div(const I &a, long long d) { return a / i(d); }
    I mod(const I &a, long long d) { return z3::mod(a, i(d)); }
};
} // namespace fsm
#endif
