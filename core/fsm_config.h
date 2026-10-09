/*
 * fsm_config.h - Parametros temporais da FSM do semaforo (PTT7)
 *
 * FONTE UNICA DE VERDADE para as temporizacoes. Este arquivo e lido:
 *   - pelo nucleo em C (fsm_core.c)  -> firmware ESP32, testes, simulador, ESBMC
 *   - pelo modelo Z3 em C++ (verification/z3/fsm_model.hpp), via #include
 *
 * Unidades: milissegundos. Valores inteiros.
 */
#ifndef FSM_CONFIG_H
#define FSM_CONFIG_H

#define FSM_T_INIT_MS           2000   /* todos vermelhos na partida               */
#define FSM_T_VERDE_MIN_MS      8000   /* tempo minimo de verde para veiculos       */
#define FSM_T_AMARELO_MS        3000   /* amarelo veicular (ATENCAO_VEICULO)        */
#define FSM_T_VERDE_PED_MS      6000   /* verde para pedestres                      */
#define FSM_T_ATENCAO_PED_MS    3000   /* vermelho piscante p/ pedestre (limpeza)   */
#define FSM_T_RECOVERY_MS       3000   /* todos vermelhos apos reset de falha       */

#define FSM_DT_MAX_MS            200   /* atraso maximo aceito entre dois passos;
                                          acima disso => falha de temporizacao     */
#define FSM_BLINK_HALF_MS        500   /* meio periodo do pisca (FALHA / ATENCAO_PED) */

#define FSM_TIMER_SAT_MS       60000   /* saturacao dos contadores (evita overflow) */

#endif /* FSM_CONFIG_H */
