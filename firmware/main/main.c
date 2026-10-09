/*
 * main.c - Invólucro (wrapper) ESP-IDF do semaforo PTT7
 *
 * Camadas:
 *   core/fsm_core.c   nucleo funcional (FSM pura, verificada por Z3/ESBMC) - componente fsm_core
 *   main.c            hardware: GPIO, ISR dos botoes, tarefa de controle FreeRTOS,
 *                     instrumentacao de latencia (esp_timer_get_time), injecao de falhas,
 *                     console serial de experimentos e tarefa de log.
 *
 * Instrumentacao (protocolo 6.3):
 *   LREC = t(apos atualizar GPIO) - t(entrada da ISR do botao), em microssegundos.
 *   Os registros sao enviados por fila a uma tarefa de log de baixa prioridade:
 *   nenhuma escrita serial ocorre dentro do trecho cronometrado.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_random.h"
#include "esp_app_desc.h"
#include "sdkconfig.h"

#include "fsm_core.h"
#include "app_config.h"

/* ======================================================================
 * Estado compartilhado
 * ==================================================================== */
static TaskHandle_t s_ctrl_task;
static QueueHandle_t s_log_q;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* eventos vindos das ISRs (protegidos por s_mux) */
static volatile uint8_t s_req[2];
static volatile int64_t s_t_isr[2];
static volatile uint8_t s_reset_req;
static volatile int64_t s_last_edge[3];

/* comandos de software (bancada) */
static volatile uint8_t s_sw_req;          /* pedido de travessia sem medicao de latencia */
static volatile uint8_t s_inj_loss1, s_inj_loss2;
static volatile uint32_t s_inj_stall_ms;   /* trava a tarefa de controle (falha temporal) */

/* publicados pela tarefa de controle */
static volatile uint8_t s_state = ST_INIT;
static volatile uint32_t s_green_ms;
static volatile uint32_t s_steps;

/* acompanhamento do experimento de falhas */
static volatile uint8_t s_inj_armed;
static volatile int64_t s_inj_t_us;
static volatile int64_t s_safe_t_us;
static volatile uint8_t s_green_after_safe;

/* identificacao do ensaio corrente (copiada nos registros de latencia) */
static volatile uint16_t s_session = 1;
static volatile uint16_t s_seq;
static volatile uint8_t s_warmup;
static volatile uint8_t s_log_tr = 1;

/* ======================================================================
 * Registros de log (formatados fora do trecho cronometrado)
 * ==================================================================== */
typedef enum { EV_TR = 1, EV_LAT = 2 } ev_type_t;
typedef struct {
    uint8_t type, from, to, tr, btn, warm;
    uint16_t session, seq;
    int64_t t_us;          /* EV_TR: instante; EV_LAT: instante da ISR */
    int32_t lat_dec_us;    /* ISR -> decisao da FSM                    */
    int32_t lat_out_us;    /* ISR -> GPIO atualizado (LREC)            */
} log_ev_t;

static void log_push(const log_ev_t *e)
{
    (void)xQueueSend(s_log_q, e, 0);   /* nunca bloqueia a tarefa de controle */
}

static void log_task(void *arg)
{
    (void)arg;
    log_ev_t e;
    for (;;) {
        if (xQueueReceive(s_log_q, &e, portMAX_DELAY) != pdTRUE) continue;
        if (e.type == EV_TR) {
            if (s_log_tr)
                printf("TR,%" PRId64 ",%s,%s,T%u\n", e.t_us, fsm_state_name(e.from), fsm_state_name(e.to), e.tr);
        } else if (e.type == EV_LAT) {
            printf("LAT,%u,%u,%u,%u,%s,T%u,%" PRId64 ",%" PRId32 ",%" PRId32 "\n", e.session, e.seq, e.warm, e.btn,
                   fsm_state_name(e.from), e.tr, e.t_us, e.lat_dec_us, e.lat_out_us);
        }
    }
}

/* ======================================================================
 * ISR dos botoes (IRAM)
 * ==================================================================== */
static void IRAM_ATTR btn_isr(void *arg)
{
    const uint32_t id = (uint32_t)arg;           /* 0 = ped1, 1 = ped2, 2 = reset */
    const int64_t t = esp_timer_get_time();
    BaseType_t hp = pdFALSE;

    portENTER_CRITICAL_ISR(&s_mux);
    const int accept = (t - s_last_edge[id]) >= DEBOUNCE_US;
    if (accept) {
        s_last_edge[id] = t;
        if (id < 2) {
            if (!s_req[id]) {                      /* guarda o instante da 1a borda */
                s_t_isr[id] = t;
                s_req[id] = 1;
            }
        } else {
            s_reset_req = 1;
        }
    }
    portEXIT_CRITICAL_ISR(&s_mux);

    if (accept) {
        vTaskNotifyGiveFromISR(s_ctrl_task, &hp);  /* acorda a tarefa de controle */
        if (hp == pdTRUE) portYIELD_FROM_ISR();
    }
}

/* ======================================================================
 * GPIO
 * ==================================================================== */
static void gpio_setup_outputs(void)
{
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_CAR_R) | (1ULL << PIN_CAR_Y) | (1ULL << PIN_CAR_G) |
                        (1ULL << PIN_PED_R) | (1ULL << PIN_PED_G),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out));

    gpio_config_t stim = {
        .pin_bit_mask = (1ULL << PIN_STIM1) | (1ULL << PIN_STIM2),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&stim));
    gpio_set_level(PIN_STIM1, 1);                  /* 1 = alta impedancia (solto) */
    gpio_set_level(PIN_STIM2, 1);
}

/* chamada DENTRO da tarefa de controle: as ISRs ficam alocadas no mesmo nucleo (1) */
static void gpio_setup_inputs_and_isr(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << PIN_BTN_PED1) | (1ULL << PIN_BTN_PED2) | (1ULL << PIN_BTN_RESET),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    gpio_config_t sig = {
        .pin_bit_mask = (1ULL << PIN_SIG1) | (1ULL << PIN_SIG2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&sig));

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BTN_PED1, btn_isr, (void *)0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BTN_PED2, btn_isr, (void *)1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BTN_RESET, btn_isr, (void *)2));
}

static inline void write_leds(const fsm_outputs_t *o)
{
    gpio_set_level(PIN_CAR_R, o->car_r);
    gpio_set_level(PIN_CAR_Y, o->car_y);
    gpio_set_level(PIN_CAR_G, o->car_g);
    gpio_set_level(PIN_PED_R, o->ped_r);
    gpio_set_level(PIN_PED_G, o->ped_g);
}

/* ======================================================================
 * Tarefa de controle (nucleo 1, alta prioridade)
 * ==================================================================== */
static void control_task(void *arg)
{
    (void)arg;
    fsm_ctx_t ctx;
    fsm_outputs_t out;

    gpio_setup_inputs_and_isr();
    fsm_init(&ctx);
    fsm_outputs(&ctx, &out);
    write_leds(&out);
    int64_t last_us = esp_timer_get_time();

    for (;;) {
        /* acorda por notificacao da ISR ou no maximo a cada CONTROL_PERIOD_MS */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONTROL_PERIOD_MS));

        const uint32_t stall = s_inj_stall_ms;     /* injecao de falha de temporizacao */
        if (stall) {
            s_inj_stall_ms = 0;
            vTaskDelay(pdMS_TO_TICKS(stall));
        }

        /* --- coleta de entradas --- */
        const int64_t now = esp_timer_get_time();
        const int64_t dt_us = now - last_us;
        const uint32_t dt_ms = (dt_us > (int64_t)UINT32_MAX * 1000) ? UINT32_MAX : (uint32_t)(dt_us / 1000);
        last_us += (int64_t)dt_ms * 1000;          /* preserva o resto em us (sem deriva) */

        uint8_t req[2], rst;
        int64_t t_isr[2];
        portENTER_CRITICAL(&s_mux);
        req[0] = s_req[0]; req[1] = s_req[1];
        t_isr[0] = s_t_isr[0]; t_isr[1] = s_t_isr[1];
        rst = s_reset_req;
        s_req[0] = s_req[1] = 0;
        s_reset_req = 0;
        portEXIT_CRITICAL(&s_mux);
        const uint8_t sw_req = s_sw_req;
        s_sw_req = 0;

        fsm_inputs_t in;
        memset(&in, 0, sizeof in);
        in.req1 = req[0] || sw_req;
        in.req2 = req[1];
        in.reset = rst;
#if PTT7_SUPERVISAO_HW
        in.sig1_ok = (gpio_get_level(PIN_SIG1) == 0) && !s_inj_loss1;
        in.sig2_ok = (gpio_get_level(PIN_SIG2) == 0) && !s_inj_loss2;
#else
        in.sig1_ok = !s_inj_loss1;
        in.sig2_ok = !s_inj_loss2;
#endif
        in.dt_ms = dt_ms;

        /* --- trecho cronometrado: FSM + atualizacao dos GPIOs --- */
        const uint8_t from = ctx.state;
        const uint8_t tr = fsm_step(&ctx, &in);
        const int64_t t_dec = esp_timer_get_time();
        fsm_outputs(&ctx, &out);
        write_leds(&out);
        const int64_t t_out = esp_timer_get_time();
        /* --- fim do trecho cronometrado --- */

        if (s_inj_armed) {
            if (s_safe_t_us == 0 && ctx.state == ST_FALHA && !out.car_g && !out.ped_g) s_safe_t_us = t_out;
            if (s_safe_t_us != 0 && (out.car_g || out.ped_g)) s_green_after_safe = 1;
        }
        s_state = ctx.state;
        s_green_ms = ctx.green_ms;
        s_steps++;

        if (tr != TR_NONE) {
            log_ev_t e = { .type = EV_TR, .from = from, .to = ctx.state, .tr = tr, .t_us = t_out };
            log_push(&e);
        }
        for (int b = 0; b < 2; b++) {
            if (!req[b]) continue;
            log_ev_t e = { .type = EV_LAT, .from = from, .to = ctx.state, .tr = tr, .btn = (uint8_t)(b + 1),
                           .warm = s_warmup, .session = s_session, .seq = s_seq, .t_us = t_isr[b],
                           .lat_dec_us = (int32_t)(t_dec - t_isr[b]), .lat_out_us = (int32_t)(t_out - t_isr[b]) };
            log_push(&e);
        }
    }
}

/* ======================================================================
 * Bancada: experimentos automaticos e console serial (nucleo 0)
 * ==================================================================== */
static void notify_ctrl(void) { xTaskNotifyGive(s_ctrl_task); }

static int wait_for(int (*pred)(void), uint32_t timeout_ms)
{
    const int64_t end = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < end) {
        if (pred()) return 1;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return pred();
}
static int st_vv_min(void) { return s_state == ST_VERDE_VEICULO && s_green_ms >= FSM_T_VERDE_MIN_MS + 100; }
static int st_not_vv(void) { return s_state != ST_VERDE_VEICULO; }
static int st_vv(void) { return s_state == ST_VERDE_VEICULO; }
static int st_vp(void) { return s_state == ST_VERMELHO_VEICULO_VERDE_PEDESTRE; }
static int st_recovery(void) { return s_state == ST_RECOVERY; }
static int st_green_any(void) { return s_state == ST_VERDE_VEICULO || s_state == ST_PEDIDO_PEDESTRE; }

static void clear_injections(void)
{
    s_inj_loss1 = 0;
    s_inj_loss2 = 0;
    s_inj_stall_ms = 0;
}

static void sw_reset(void)
{
    portENTER_CRITICAL(&s_mux);
    s_reset_req = 1;
    portEXIT_CRITICAL(&s_mux);
    notify_ctrl();
}

/* leva o sistema de volta a VERDE_VEICULO (limpa falhas e reseta se necessario) */
static int back_to_normal(void)
{
    clear_injections();
    if (s_state == ST_FALHA) {
        vTaskDelay(pdMS_TO_TICKS(50));
        sw_reset();
    }
    return wait_for(st_vv, FSM_T_INIT_MS + FSM_T_RECOVERY_MS + FSM_T_AMARELO_MS + FSM_T_VERDE_PED_MS +
                               FSM_T_ATENCAO_PED_MS + 2000);
}

static uint32_t rand_range(uint32_t lo, uint32_t hi) { return lo + (esp_random() % (hi - lo + 1)); }

/* Q2 - latencia ISR -> GPIO (LREC) usando o estimulo controlado STIMx -> BTN_PEDx */
static void run_latency(int n, int warm, int btn)
{
    const int stim = (btn == 2) ? PIN_STIM2 : PIN_STIM1;
    printf("#INICIO lat sessao=%u n=%d aquecimento=%d botao=%d\n", s_session, n, warm, btn);
    for (int i = 0; i < warm + n; i++) {
        if (!back_to_normal() || !wait_for(st_vv_min, FSM_T_VERDE_MIN_MS + 2000)) {
            printf("#AVISO ensaio %d: sistema nao voltou a VERDE_VEICULO com verde minimo; abortando\n", i);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(rand_range(20, 400)));     /* defasagem aleatoria em relacao ao periodo */
        s_seq = (uint16_t)i;
        s_warmup = (i < warm);
        gpio_set_level(stim, 0);
        vTaskDelay(pdMS_TO_TICKS(STIM_PULSE_MS));
        gpio_set_level(stim, 1);
        if (!wait_for(st_not_vv, 2000)) {
            printf("#DESCARTE,lat,%u,%d,estimulo nao detectado (verifique jumper STIM%d->BTN_PED%d)\n",
                   s_session, i, btn, btn);
            continue;
        }
        wait_for(st_vv, FSM_T_AMARELO_MS + FSM_T_VERDE_PED_MS + FSM_T_ATENCAO_PED_MS + 2000);
    }
    s_warmup = 0;
    vTaskDelay(pdMS_TO_TICKS(200));
    printf("#FIM lat\n");
}

/* Q3 - injecao de falhas: 1 = perda sinal botao 1, 2 = perda sinal botao 2,
 *      3 = perda simultanea durante VERMELHO_VEICULO_VERDE_PEDESTRE, 4 = falha de temporizacao */
static void run_faults(int tipo, int n)
{
    static const char *nomes[] = { "?", "perda_sinal_botao1", "perda_sinal_botao2",
                                   "perda_simultanea_VP", "falha_temporizacao" };
    printf("#INICIO falha sessao=%u tipo=%d(%s) n=%d janela_ms=%d\n", s_session, tipo, nomes[tipo], n, FALLBACK_WINDOW_MS);
    printf("#FAULT,sessao,seq,tipo,estado_injecao,t_inj_us,conv_ms,sucesso,verde_apos_falha,plataforma\n");
    printf("#REC,sessao,seq,tipo,sucesso,ms_ate_recovery,ms_ate_verde\n");
    for (int i = 0; i < n; i++) {
        if (!back_to_normal()) {
            printf("#AVISO ensaio %d: sistema nao voltou ao normal; abortando\n", i);
            break;
        }
        if (tipo == 3) {                                     /* leva o sistema a VP */
            wait_for(st_vv_min, FSM_T_VERDE_MIN_MS + 2000);
            s_sw_req = 1;
            notify_ctrl();
            if (!wait_for(st_vp, FSM_T_AMARELO_MS + 2000)) {
                printf("#DESCARTE,falha,%u,%d,nao atingiu VP antes da injecao\n", s_session, i);
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(rand_range(50, FSM_T_VERDE_PED_MS - 1500)));
        } else {
            vTaskDelay(pdMS_TO_TICKS(rand_range(200, 3000)));
            if (!st_green_any()) {
                printf("#DESCARTE,falha,%u,%d,estado fora do previsto antes da injecao\n", s_session, i);
                continue;
            }
        }

        const uint8_t st_inj = s_state;
        s_safe_t_us = 0;
        s_green_after_safe = 0;
        s_inj_t_us = esp_timer_get_time();
        s_inj_armed = 1;
        switch (tipo) {                                     /* injecao (sem notificar: amostragem natural) */
        case 1: s_inj_loss1 = 1; break;
        case 2: s_inj_loss2 = 1; break;
        case 3: s_inj_loss1 = 1; s_inj_loss2 = 1; break;
        default: s_inj_stall_ms = FSM_DT_MAX_MS + 100; break;
        }
        vTaskDelay(pdMS_TO_TICKS(FALLBACK_WINDOW_MS));      /* janela de observacao */
        s_inj_armed = 0;

        const int64_t safe = s_safe_t_us;
        const double conv_ms = safe ? (double)(safe - s_inj_t_us) / 1000.0 : -1.0;
        const int ok = safe && conv_ms <= FALLBACK_WINDOW_MS && !s_green_after_safe && s_state == ST_FALHA;
        printf("FAULT,%u,%d,%d,%s,%" PRId64 ",%.3f,%d,%d,esp32\n", s_session, i, tipo, fsm_state_name(st_inj),
               s_inj_t_us, conv_ms, ok, s_green_after_safe);

        /* recuperacao via reset (avaliada separadamente; exige entradas validas) */
        clear_injections();
        vTaskDelay(pdMS_TO_TICKS(200));
        const int64_t t_reset = esp_timer_get_time();
        sw_reset();
        const int r1 = wait_for(st_recovery, 1000);
        const int64_t t_rec = esp_timer_get_time();
        const int r2 = r1 && wait_for(st_vv, FSM_T_RECOVERY_MS + 1000);
        const int64_t t_vv = esp_timer_get_time();
        printf("REC,%u,%d,%d,%d,%.1f,%.1f\n", s_session, i, tipo, r1 && r2,
               r1 ? (double)(t_rec - t_reset) / 1000.0 : -1.0, r2 ? (double)(t_vv - t_reset) / 1000.0 : -1.0);
    }
    printf("#FIM falha\n");
}

static void print_header(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    printf("\n# PTT7 semaforo FSM | firmware %s | ESP-IDF %s | compilado %s %s\n", d->version, d->idf_ver, d->date, d->time);
    printf("# cpu_mhz=%d freertos_hz=%d periodo_ms=%d debounce_us=%d supervisao_hw=%d core_iram=%s\n",
           CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, CONFIG_FREERTOS_HZ, CONTROL_PERIOD_MS, DEBOUNCE_US, PTT7_SUPERVISAO_HW,
#ifdef CONFIG_PTT7_CORE_IN_IRAM
           "sim"
#else
           "nao"
#endif
    );
    printf("# T_INIT=%d T_VERDE_MIN=%d T_AMARELO=%d T_VERDE_PED=%d T_ATENCAO_PED=%d T_RECOVERY=%d DT_MAX=%d\n",
           FSM_T_INIT_MS, FSM_T_VERDE_MIN_MS, FSM_T_AMARELO_MS, FSM_T_VERDE_PED_MS, FSM_T_ATENCAO_PED_MS,
           FSM_T_RECOVERY_MS, FSM_DT_MAX_MS);
    printf("# formatos: TR,t_us,de,para,Tn | LAT,sessao,seq,aquecimento,botao,estado_origem,Tn,t_isr_us,lat_decisao_us,lat_saida_us\n");
    printf("#           FAULT,sessao,seq,tipo,estado_injecao,t_inj_us,conv_ms,sucesso,verde_apos_falha,plataforma | REC,...\n");
    printf("# digite 'ajuda' para a lista de comandos\n");
}

static void print_help(void)
{
    printf("Comandos:\n"
           "  status                      estado atual e injecoes ativas\n"
           "  sessao <n>                  define o numero da sessao (1..3)\n"
           "  lat <n> [aquec=5] [botao=1] Q2: n medicoes de latencia + aquecimento (requer jumper STIM->BTN)\n"
           "  falha <tipo> <n>            Q3: n injecoes; tipo 1=sinal btn1, 2=sinal btn2, 3=ambos em VP, 4=temporizacao\n"
           "  stim <1|2>                  um pulso de estimulo no botao (gera LAT)\n"
           "  req                         pedido de travessia por software\n"
           "  perda <1|2|12> / restaura   injeta/remove perda de sinal manualmente\n"
           "  atraso <ms>                 trava a tarefa de controle por <ms> (falha temporal se > %d)\n"
           "  reset                       reset por software\n"
           "  tr <0|1>                    liga/desliga log de transicoes (TR)\n", FSM_DT_MAX_MS);
}

static void handle_cmd(char *line)
{
    char *argv[6] = { 0 };
    int argc = 0;
    for (char *tok = strtok(line, " \t"); tok && argc < 6; tok = strtok(NULL, " \t")) argv[argc++] = tok;
    if (argc == 0) return;
    const char *c = argv[0];
    if (!strcmp(c, "ajuda") || !strcmp(c, "help")) print_help();
    else if (!strcmp(c, "status"))
        printf("#STATUS estado=%s green_ms=%" PRIu32 " passos=%" PRIu32 " perda1=%u perda2=%u sessao=%u\n",
               fsm_state_name(s_state), s_green_ms, s_steps, s_inj_loss1, s_inj_loss2, s_session);
    else if (!strcmp(c, "sessao") && argc > 1) { s_session = (uint16_t)atoi(argv[1]); printf("#OK sessao=%u\n", s_session); }
    else if (!strcmp(c, "lat") && argc > 1)
        run_latency(atoi(argv[1]), argc > 2 ? atoi(argv[2]) : 5, argc > 3 ? atoi(argv[3]) : 1);
    else if (!strcmp(c, "falha") && argc > 2 && atoi(argv[1]) >= 1 && atoi(argv[1]) <= 4)
        run_faults(atoi(argv[1]), atoi(argv[2]));
    else if (!strcmp(c, "stim") && argc > 1) {
        const int pin = atoi(argv[1]) == 2 ? PIN_STIM2 : PIN_STIM1;
        gpio_set_level(pin, 0);
        vTaskDelay(pdMS_TO_TICKS(STIM_PULSE_MS));
        gpio_set_level(pin, 1);
    } else if (!strcmp(c, "req")) { s_sw_req = 1; notify_ctrl(); }
    else if (!strcmp(c, "perda") && argc > 1) {
        if (strchr(argv[1], '1')) s_inj_loss1 = 1;
        if (strchr(argv[1], '2')) s_inj_loss2 = 1;
        printf("#OK perda1=%u perda2=%u\n", s_inj_loss1, s_inj_loss2);
    } else if (!strcmp(c, "restaura")) { clear_injections(); printf("#OK injecoes removidas\n"); }
    else if (!strcmp(c, "atraso") && argc > 1) { s_inj_stall_ms = (uint32_t)atoi(argv[1]); }
    else if (!strcmp(c, "reset")) sw_reset();
    else if (!strcmp(c, "tr") && argc > 1) s_log_tr = (uint8_t)(atoi(argv[1]) != 0);
    else printf("#ERRO comando invalido: %s (digite 'ajuda')\n", c);
}

static void bench_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
    print_header();
    char line[96];
    size_t len = 0;
    for (;;) {
        uint8_t ch;
        if (uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(50)) != 1) continue;
        if (ch == '\r' || ch == '\n') {
            if (len) {
                line[len] = 0;
                printf("\n");
                handle_cmd(line);
                len = 0;
            }
        } else if (len < sizeof line - 1) {
            line[len++] = (char)ch;
            putchar(ch);                                  /* eco */
            fflush(stdout);
        }
    }
}

void app_main(void)
{
    s_log_q = xQueueCreate(64, sizeof(log_ev_t));
    gpio_setup_outputs();
    xTaskCreatePinnedToCore(log_task, "log", 4096, NULL, LOG_TASK_PRIO, NULL, AUX_TASK_CORE);
    xTaskCreatePinnedToCore(control_task, "fsm_ctrl", 4096, NULL, CTRL_TASK_PRIO, &s_ctrl_task, CTRL_TASK_CORE);
    xTaskCreatePinnedToCore(bench_task, "bancada", 6144, NULL, BENCH_TASK_PRIO, NULL, AUX_TASK_CORE);
}
