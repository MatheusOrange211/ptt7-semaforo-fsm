// verify_z3.cpp - Camada 1 da verificacao: consultas Z3 (API C++) sobre o modelo da FSM.
//
// Uso:
//   verify_z3 [--k 10] [--timeout 120] [--mutante] [--out verification/results/z3_results.json]
//
// Categorias de resultado:
//   PROVADO           consulta de violacao UNSAT sobre TODOS os contextos validos (1 passo
//                     + preservacao do dominio => vale para qualquer execucao: argumento indutivo)
//   ALCANCADO         consulta existencial SAT: caminho encontrado (P1)
//   VALIDO_NO_LIMITE  nenhuma violacao ate o horizonte k (BMC)
//   REFUTADO          contraexemplo encontrado (impresso e salvo no JSON)
//   INCONCLUSIVO      unknown/timeout, ou caminho nao encontrado ate k
#define FSM_MODEL_WITH_Z3
#include "fsm_model.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace fsm;
using CtxZ = Ctx<Z3Ops>;
using InpZ = Inp<Z3Ops>;
using InfoZ = Info<Z3Ops>;

struct Args {
    int k = 10;
    unsigned timeout_s = 120;
    bool mutante = false;
    std::string out;
};

struct Result {
    std::string id, desc, status, detail, cex_json;
    double secs = 0;
    int bound = -1;
};

static std::string jesc(const std::string &s)
{
    std::string r;
    for (char ch : s) {
        if (ch == '"' || ch == '\\') { r += '\\'; r += ch; }
        else if (ch == '\n') r += "\\n";
        else r += ch;
    }
    return r;
}

class Verifier {
public:
    explicit Verifier(const Args &a) : args(a), o(zc) {}

    int run();

private:
    Args args;
    z3::context zc;
    Z3Ops o;
    std::vector<Result> results;
    int counter = 0;

    CtxZ sym_ctx(const std::string &tag)
    {
        return CtxZ{zc.int_const(("s_" + tag).c_str()), zc.int_const(("el_" + tag).c_str()),
                    zc.int_const(("gr_" + tag).c_str()), zc.bool_const(("pend_" + tag).c_str())};
    }
    InpZ sym_inp(const std::string &tag)
    {
        return InpZ{zc.bool_const(("req1_" + tag).c_str()), zc.bool_const(("req2_" + tag).c_str()),
                    zc.bool_const(("sig1_" + tag).c_str()), zc.bool_const(("sig2_" + tag).c_str()),
                    zc.bool_const(("reset_" + tag).c_str()), zc.int_const(("dt_" + tag).c_str())};
    }
    z3::expr ctx_valid(const CtxZ &c)
    {
        return c.s >= 0 && c.s < NUM_STATES && c.el >= 0 && c.el <= FSM_TIMER_SAT_MS &&
               c.gr >= 0 && c.gr <= FSM_TIMER_SAT_MS;
    }
    // dominio admissivel: dt e um uint32 qualquer (inclui atrasos enormes = falha)
    z3::expr inp_valid(const InpZ &i) { return i.dt >= 0 && i.dt <= zc.int_val(int64_t(4294967295LL)); }
    z3::expr ctx_initial(const CtxZ &c) { return c.s == INIT && !c.pend && c.el == 0 && c.gr == 0; }

    InfoZ blank_info() { return InfoZ{zc.bool_val(false), zc.int_val(0), zc.int_val(0), {}}; }

    z3::solver new_solver()
    {
        z3::solver s(zc);
        z3::params p(zc);
        p.set("timeout", args.timeout_s * 1000u);
        s.set(p);
        return s;
    }

    static long long ival(z3::model &m, const z3::expr &e)
    {
        return m.eval(e, true).get_numeral_int64();
    }
    static bool bval(z3::model &m, const z3::expr &e) { return m.eval(e, true).is_true(); }

    std::string ctx_json(z3::model &m, const CtxZ &c)
    {
        std::ostringstream ss;
        long long s = ival(m, c.s);
        ss << "{\"estado\":\"" << state_name(s) << "\",\"elapsed_ms\":" << ival(m, c.el)
           << ",\"green_ms\":" << ival(m, c.gr) << ",\"pending\":" << (bval(m, c.pend) ? "true" : "false") << "}";
        return ss.str();
    }
    std::string inp_json(z3::model &m, const InpZ &i)
    {
        std::ostringstream ss;
        ss << "{\"req1\":" << bval(m, i.req1) << ",\"req2\":" << bval(m, i.req2)
           << ",\"sig1_ok\":" << bval(m, i.sig1_ok) << ",\"sig2_ok\":" << bval(m, i.sig2_ok)
           << ",\"reset\":" << bval(m, i.reset) << ",\"dt_ms\":" << ival(m, i.dt) << "}";
        return ss.str();
    }

    void record(Result r)
    {
        const char *mark = r.status == "PROVADO" || r.status == "ALCANCADO" ? "OK "
                         : r.status == "VALIDO_NO_LIMITE" ? "OK*"
                         : r.status == "REFUTADO" ? "XX " : "?? ";
        std::printf("[%s] %-6s %-17s %s  %s\n", mark, r.id.c_str(), r.status.c_str(), r.desc.c_str(), r.detail.c_str());
        if (!r.cex_json.empty()) std::printf("        contraexemplo: %s\n", r.cex_json.c_str());
        std::fflush(stdout);
        results.push_back(r);
    }

    // ------------------------------------------------- consultas de 1 passo
    using ViolFn = std::function<z3::expr(const CtxZ &, const InpZ &, const CtxZ &, const InfoZ &)>;

    void one_step(const std::string &id, const std::string &desc, ViolFn viol)
    {
        auto t0 = std::chrono::steady_clock::now();
        std::string tag = "q" + std::to_string(counter++);
        CtxZ c = sym_ctx(tag);
        InpZ i = sym_inp(tag);
        InfoZ info = blank_info();
        z3::expr tr = zc.int_val(0);
        CtxZ n = model_step(o, c, i, args.mutante, &info, &tr);
        z3::solver s = new_solver();
        s.add(ctx_valid(c));
        s.add(inp_valid(i));
        s.add(viol(c, i, n, info));
        z3::check_result r = s.check();
        Result res{id, desc, "", "", "", secs_since(t0), -1};
        if (r == z3::unsat) {
            res.status = "PROVADO";
            res.detail = "(todos os contextos validos, 1 passo)";
        } else if (r == z3::sat) {
            z3::model m = s.get_model();
            res.status = "REFUTADO";
            res.cex_json = "{\"contexto\":" + ctx_json(m, c) + ",\"entrada\":" + inp_json(m, i) +
                           ",\"proximo_estado\":\"" + state_name(ival(m, n.s)) + "\"}";
        } else {
            res.status = "INCONCLUSIVO";
            res.detail = "z3: " + s.reason_unknown();
        }
        record(res);
    }

    // ------------------------------------------------------- desdobramento
    struct Unrolled {
        std::vector<CtxZ> ctxs;
        std::vector<InpZ> inps;
        std::vector<InfoZ> infos;
    };

    Unrolled unroll(z3::solver &s, int k, const std::function<z3::expr(const CtxZ &)> &start)
    {
        Unrolled u;
        std::string base = "u" + std::to_string(counter++) + "_";
        u.ctxs.push_back(sym_ctx(base + "0"));
        s.add(start ? start(u.ctxs[0]) : ctx_initial(u.ctxs[0]));
        for (int j = 0; j < k; j++) {
            InpZ in = sym_inp(base + std::to_string(j));
            InfoZ info = blank_info();
            CtxZ n = model_step(o, u.ctxs.back(), in, args.mutante, &info, nullptr);
            CtxZ nx = sym_ctx(base + std::to_string(j + 1));
            s.add(inp_valid(in));
            s.add(nx.s == n.s && nx.el == n.el && nx.gr == n.gr && nx.pend == n.pend);
            u.inps.push_back(in);
            u.infos.push_back(info);
            u.ctxs.push_back(nx);
        }
        return u;
    }

    static double secs_since(std::chrono::steady_clock::time_point t0)
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }

    // P5: para todo caminho de ate k passos desde INIT, falha no passo j => FALHA em j+1
    void p5_bmc()
    {
        auto t0 = std::chrono::steady_clock::now();
        z3::solver s = new_solver();
        Unrolled u = unroll(s, args.k, nullptr);
        z3::expr_vector viol(zc);
        for (int j = 0; j < args.k; j++) viol.push_back(u.infos[j].fault && u.ctxs[j + 1].s != FALHA);
        s.add(z3::mk_or(viol));
        z3::check_result r = s.check();
        Result res{"P5", "falha no passo j => FALHA em j+1 (<= 1 periodo de controle) e mantido enquanto persistir",
                   "", "", "", secs_since(t0), args.k};
        if (r == z3::unsat) {
            res.status = "VALIDO_NO_LIMITE";
            res.detail = "(BMC k=" + std::to_string(args.k) + "; complemento indutivo: P4 e P6 provados)";
        } else if (r == z3::sat) {
            z3::model m = s.get_model();
            res.status = "REFUTADO";
            std::string cx = "[";
            for (int j = 0; j <= args.k; j++) cx += (j ? "," : "") + ctx_json(m, u.ctxs[j]);
            res.cex_json = cx + "]";
        } else {
            res.status = "INCONCLUSIVO";
            res.detail = s.reason_unknown();
        }
        record(res);
    }

    // P1: existe sequencia de entradas admissiveis SEM FALHA (exceto alvo FALHA/RECOVERY)
    // levando INIT ao estado alvo em <= k passos.
    void reach_from_init(int target, int k)
    {
        auto t0 = std::chrono::steady_clock::now();
        z3::solver s = new_solver();
        Unrolled u = unroll(s, k, nullptr);
        bool allow_fault = (target == FALHA || target == RECOVERY);
        if (!allow_fault)
            for (auto &in : u.inps) s.add(in.sig1_ok && in.sig2_ok && in.dt <= FSM_DT_MAX_MS);
        z3::expr_vector hit(zc);
        for (auto &c : u.ctxs) hit.push_back(c.s == target);
        s.add(z3::mk_or(hit));
        z3::check_result r = s.check();
        Result res{"P1." + std::to_string(target), std::string(state_name(target)) + " alcancavel a partir de INIT",
                   "", "", "", 0, k};
        if (r == z3::sat) {
            z3::model m = s.get_model();
            size_t first = 0;
            while (ival(m, u.ctxs[first].s) != target) first++;
            std::string path;
            long long prev = -1;
            for (size_t j = 0; j <= first; j++) {
                long long st = ival(m, u.ctxs[j].s);
                if (st != prev) { path += (path.empty() ? "" : " -> ") + std::string(state_name(st)); prev = st; }
            }
            res.status = "ALCANCADO";
            res.detail = "caminho de " + std::to_string(first) + " passos: " + path;
        } else if (r == z3::unsat) {
            res.status = "INCONCLUSIVO";
            res.detail = "nao alcancado ate k=" + std::to_string(k);
        } else {
            res.status = "INCONCLUSIVO";
            res.detail = s.reason_unknown();
        }
        res.secs = secs_since(t0);
        record(res);
    }

    // P2c: ausencia de deadlock global - de TODO contexto valido do estado `from`, a estrategia
    // de entradas (sem falha, dt = DT_MAX, reset e pedido acionados) leva a VERDE_VEICULO em <= k.
    void back_to_green(int from, int k)
    {
        auto t0 = std::chrono::steady_clock::now();
        z3::solver s = new_solver();
        Unrolled u = unroll(s, k, [&](const CtxZ &c) { return ctx_valid(c) && c.s == from; });
        for (auto &in : u.inps)
            s.add(in.sig1_ok && in.sig2_ok && in.reset && in.req1 && !in.req2 && in.dt == FSM_DT_MAX_MS);
        z3::expr_vector never(zc);
        for (auto &c : u.ctxs) never.push_back(c.s != VV);
        s.add(z3::mk_and(never));   // procura contexto inicial que NAO volta a VV
        z3::check_result r = s.check();
        Result res{"P2c." + std::to_string(from),
                   std::string("de ") + state_name(from) + " (contexto arbitrario) retorna a VERDE_VEICULO",
                   "", "", "", 0, k};
        if (r == z3::unsat) {
            res.status = "PROVADO";
            res.detail = "(para todo contexto valido do estado, em <= " + std::to_string(k) + " passos)";
        } else if (r == z3::sat) {
            z3::model m = s.get_model();
            res.status = "INCONCLUSIVO";
            res.detail = "contexto que nao retorna ate k=" + std::to_string(k);
            res.cex_json = ctx_json(m, u.ctxs[0]);
        } else {
            res.status = "INCONCLUSIVO";
            res.detail = s.reason_unknown();
        }
        res.secs = secs_since(t0);
        record(res);
    }

    // numero de passos suficiente para percorrer o ciclo completo com dt = DT_MAX (+ folga)
    static int cycle_bound()
    {
        auto ceil_div = [](long a, long b) { return int((a + b - 1) / b); };
        const long d = FSM_DT_MAX_MS;
        return ceil_div(FSM_T_INIT_MS, d) + ceil_div(FSM_T_VERDE_MIN_MS, d) + ceil_div(FSM_T_AMARELO_MS, d) +
               ceil_div(FSM_T_VERDE_PED_MS, d) + ceil_div(FSM_T_ATENCAO_PED_MS, d) +
               ceil_div(FSM_T_RECOVERY_MS, d) + 8;
    }

    void save(int kcyc);
};

int Verifier::run()
{
    unsigned maj, min, build, rev;
    Z3_get_version(&maj, &min, &build, &rev);
    const int kcyc = cycle_bound();
    std::printf("Z3 %u.%u.%u (API C++) | horizonte BMC P5 k=%d | horizonte de ciclo k_ciclo=%d%s\n",
                maj, min, build, args.k, kcyc, args.mutante ? " | MODO MUTANTE (FALHA->VP)" : "");
    std::printf("Parametros (core/fsm_config.h): T_INIT=%d T_VERDE_MIN=%d T_AMARELO=%d T_VERDE_PED=%d "
                "T_ATENCAO_PED=%d T_RECOVERY=%d DT_MAX=%d BLINK=%d SAT=%d\n",
                FSM_T_INIT_MS, FSM_T_VERDE_MIN_MS, FSM_T_AMARELO_MS, FSM_T_VERDE_PED_MS,
                FSM_T_ATENCAO_PED_MS, FSM_T_RECOVERY_MS, FSM_DT_MAX_MS, FSM_BLINK_HALF_MS, FSM_TIMER_SAT_MS);
    std::printf("%s\n", std::string(100, '-').c_str());

    // P2a - determinismo: guardas da tabela mutuamente exclusivas em cada estado
    one_step("P2a", "guardas da tabela mutuamente exclusivas por estado (determinismo)",
             [&](const CtxZ &c, const InpZ &, const CtxZ &, const InfoZ &info) {
                 z3::expr_vector ov(zc);
                 for (size_t a = 0; a < info.rows.size(); a++)
                     for (size_t b = a + 1; b < info.rows.size(); b++)
                         if (info.rows[a].orig == info.rows[b].orig)
                             ov.push_back(c.s == info.rows[a].orig && info.rows[a].guard && info.rows[b].guard);
                 return z3::mk_or(ov);
             });
    // P2b - totalidade + preservacao do dominio (invariante indutivo)
    one_step("P2b", "proximo estado definido para toda entrada e contexto permanece no dominio valido",
             [&](const CtxZ &, const InpZ &, const CtxZ &n, const InfoZ &) { return !ctx_valid(n); });
    // P3 - exclusao mutua
    one_step("P3", "nunca verde/amarelo veicular com verde de pedestre; verde de pedestre so em VP",
             [&](const CtxZ &c, const InpZ &, const CtxZ &n, const InfoZ &) {
                 auto bad = [&](const CtxZ &x) {
                     Out<Z3Ops> q = model_outputs(o, x);
                     return (q.ped_g && (q.car_g || q.car_y)) || (q.ped_g && x.s != VP);
                 };
                 return bad(c) || bad(n);
             });
    // P3b - tempo minimo de verde veicular
    one_step("P3b", "verde veicular so termina (sem falha) apos T_VERDE_MIN",
             [&](const CtxZ &c, const InpZ &, const CtxZ &n, const InfoZ &info) {
                 return (c.s == VV || c.s == PP) && n.s == AV && info.gr1 < FSM_T_VERDE_MIN_MS;
             });
    // P4 - violacao das entradas => FALHA
    one_step("P4", "entrada com falha (sinal perdido ou dt > DT_MAX) leva a FALHA no proximo passo",
             [&](const CtxZ &, const InpZ &, const CtxZ &n, const InfoZ &info) { return info.fault && n.s != FALHA; });
    // P6 - de FALHA so para RECOVERY
    one_step("P6", "de FALHA so se sai para RECOVERY (nunca direto para VP)",
             [&](const CtxZ &c, const InpZ &, const CtxZ &n, const InfoZ &) {
                 return c.s == FALHA && n.s != FALHA && n.s != RECOVERY;
             });
    // P5 - convergencia: passo indutivo (FALHA e mantido enquanto a falha persistir) + BMC
    one_step("P5i", "passo indutivo: em FALHA com falha persistente o proximo estado e FALHA (com P4 => convergencia em 1 passo para todo k)",
             [&](const CtxZ &c, const InpZ &, const CtxZ &n, const InfoZ &info) {
                 return c.s == FALHA && info.fault && n.s != FALHA;
             });
    p5_bmc();
    // P1 - alcancabilidade
    for (int st = 0; st < NUM_STATES; st++) reach_from_init(st, kcyc);
    // P2c - ausencia de deadlock global
    for (int st = 0; st < NUM_STATES; st++)
        if (st != VV) back_to_green(st, kcyc);

    save(kcyc);
    int bad = 0;
    for (auto &r : results) bad += (r.status == "REFUTADO" || r.status == "INCONCLUSIVO");
    std::printf("%s\nResumo Z3: %zu consultas, %d refutadas/inconclusivas\n", std::string(100, '-').c_str(),
                results.size(), bad);
    return bad ? 1 : 0;
}

void Verifier::save(int kcyc)
{
    std::string path = args.out;
    if (path.empty())
        path = args.mutante ? "verification/results/z3_results_mutante.json" : "verification/results/z3_results.json";
    std::ofstream f(path);
    if (!f) { std::fprintf(stderr, "aviso: nao foi possivel gravar %s\n", path.c_str()); return; }
    unsigned maj, min, build, rev;
    Z3_get_version(&maj, &min, &build, &rev);
    std::time_t now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    f << "{\n  \"ferramenta\": \"Z3 " << maj << "." << min << "." << build << " (API C++)\",\n"
      << "  \"camada\": \"modelo\",\n  \"mutante\": " << (args.mutante ? "true" : "false") << ",\n"
      << "  \"data\": \"" << ts << "\",\n  \"horizonte_bmc\": " << args.k << ",\n  \"horizonte_ciclo\": " << kcyc << ",\n"
      << "  \"resultados\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        const Result &r = results[i];
        f << "    {\"propriedade\": \"" << r.id << "\", \"descricao\": \"" << jesc(r.desc) << "\", \"status\": \""
          << r.status << "\", \"detalhe\": \"" << jesc(r.detail) << "\", \"tempo_s\": " << r.secs;
        if (r.bound >= 0) f << ", \"horizonte_k\": " << r.bound;
        if (!r.cex_json.empty()) f << ", \"contraexemplo\": " << r.cex_json;
        f << "}" << (i + 1 < results.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    std::printf("Resultados salvos em %s\n", path.c_str());
}

int main(int argc, char **argv)
{
    Args a;
    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        if (s == "--k" && i + 1 < argc) a.k = std::atoi(argv[++i]);
        else if (s == "--timeout" && i + 1 < argc) a.timeout_s = unsigned(std::atoi(argv[++i]));
        else if (s == "--mutante") a.mutante = true;
        else if (s == "--out" && i + 1 < argc) a.out = argv[++i];
        else {
            std::printf("uso: %s [--k N] [--timeout S] [--mutante] [--out arquivo.json]\n", argv[0]);
            return 2;
        }
    }
    try {
        Verifier v(a);
        return v.run();
    } catch (z3::exception &e) {
        std::fprintf(stderr, "erro Z3: %s\n", e.msg());
        return 3;
    }
}
