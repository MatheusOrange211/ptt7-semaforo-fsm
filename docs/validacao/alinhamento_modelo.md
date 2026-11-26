# Alinhamento do modelo da máquina — núcleo verificado × simulação Wokwi × bancada

Situação em 21/11/2026, com o repositório no commit `b4fcf0e` (GitHub e cópia local iguais).
**Atualização de 25/11/2026:** decisão tomada pela **Opção B** (seção 3); o núcleo em `core/` não mudou desde `412b609`.

## Por que isso importa

O contrato exige, no item 6.1, que **o mesmo arquivo `fsm_core.c` seja usado na compilação do
firmware e na análise com ESBMC**. No item 6.3, aponta como principal ameaça a *divergência entre
modelo, código e execução física*. Se o que roda na demonstração não é o arquivo que foi
verificado, as provas P1–P6 não valem para a demonstração.

Exemplo simples: é como provar que uma receita está certa e depois cozinhar outra parecida. O
bolo pode até sair bom, mas a prova não fala sobre ele.

## Quem é a fonte da verdade

| Artefato | Papel | Situação |
|---|---|---|
| `spec/transition_table.csv` | especificação comum (8 estados, T1–T16) | referência |
| `core/fsm_core.c` / `.h`, `core/fsm_config.h` | **único núcleo verificado** (Z3 por conformidade, ESBMC diretamente) | alinhado com a tabela |
| `firmware/` (ESP-IDF) | compila `core/` via componente `fsm_core` | alinhado |
| `sim/` (Plano B, host) | compila `core/` | alinhado |
| `simulação/` (Wokwi, Arduino) | **tem núcleo próprio**, reescrito | **divergente** |

## 1. Divergências de comportamento: `simulação/fsm_core.c` × `core/fsm_core.c`

| # | Ponto | Núcleo verificado (`core/`) | Wokwi (`simulação/`) | Efeito |
|---|---|---|---|---|
| D1 | Tempos (ms) | INIT 2000, verde mín. 8000, amarelo 3000, verde ped. 6000, atenção ped. 3000, recovery 3000 | 1000, 5000, 2000, 5000, 2000, 1000 | medições e vídeo não batem com a especificação |
| D2 | Limite de atraso (`DT_MAX`) | 200 ms | 250 ms | um atraso de 220 ms é falha em um e normal no outro |
| D3 | Destino da RECOVERY (T15) | VERDE_VEICULO | INIT | transição fora da tabela |
| D4 | Reset | só em FALHA (T14) | em **qualquer** estado leva a RECOVERY | cria transições que não existem na tabela (ex.: VP → RECOVERY) |
| D5 | Pedido com verde mínimo já cumprido (T4) | vai direto para ATENCAO_VEICULO | passa antes por PEDIDO_PEDESTRE | T4 não existe no Wokwi |
| D6 | Memória do pedido (`pending`) | pedido feito em INIT, PP, AV, AP ou RECOVERY é guardado | só aceita pedido em VERDE_VEICULO; os outros se perdem | pedestre pode "apertar e não ser atendido" |
| D7 | Entradas | `req1`, `req2`, `sig1_ok`, `sig2_ok`, `reset`, `dt_ms` | `request`, `fault` (agregados), `reset`, `dt_ms` | não dá para distinguir os tipos de falha do contrato |
| D8 | Saturação dos contadores | 60 000 ms (domínio provado, A4) | `UINT32_MAX` | domínio diferente do verificado |
| D9 | Pisca de pedestre | 500 ms por meia-onda | 250 ms | só visual |

**Conclusão:** os resultados de P1–P6 **não** se aplicam ao código do Wokwi. Hoje o Wokwi é
uma demonstração funcional separada, e não evidência do núcleo verificado.

## 2. Divergências de pinagem: Wokwi × bancada (`firmware/main/app_config.h`)

| Função | Firmware / bancada | Wokwi (`diagram.json`) |
|---|---|---|
| LED veículo VM / AM / VD | 25 / 26 / 27 | 25 / 26 / 27 (igual) |
| LED pedestre VM / VD | 18 / 19 | 18 / 19 (igual) |
| Botão pedestre 1 / 2 | **33 / 32** | **32 / 33** (invertidos em relação ao firmware: `b1` "PEDIDO 1" em D32, `b2` em D33) |
| Botão reset | **21** | **23** |
| Supervisão do sinal 1 / 2 | 22 / 23 (configurados, mas **ignorados** com `PTT7_SUPERVISAO_HW=0`; jumper ao GND só com HW=1) | não existe |
| Estímulo controlado (Q2) | 16 → 33, 17 → 32 | não existe |

Conferência de 24/11/2026 contra `simulação/diagram.json`: os LEDs de veículo e de pedestre coincidem
com o firmware; **não** coincidem os botões de pedestre (pedido 1/2 em 32/33 no Wokwi contra 33/32 no
firmware) nem o reset (23 no Wokwi contra 21 no firmware). O `diagram.json` não foi alterado.

**Atenção para a bancada (possível causa do "problema do GND"):**
- Se a protoboard for montada pelo desenho do Wokwi e gravada com o firmware ESP-IDF, os botões de
  pedestre 1 e 2 ficam trocados e o reset fica no GPIO 23, que o firmware não lê como reset.
- Com `PTT7_SUPERVISAO_HW=0` (usado na bancada de 24/11/2026) os GPIO 22/23 são ignorados e não levam
  jumper. Só com `PTT7_SUPERVISAO_HW=1` os GPIO 22 e 23 precisam de jumper ao GND; sem ele o núcleo
  entende "perda de sinal do botão" e vai para FALHA (amarelo piscante) e não sai, nem com reset. No
  Wokwi, o GPIO 23 é o botão de reset; ligado ao GND o tempo todo, ele mantém o reset pressionado.
- Todos os GNDs (cátodos dos LEDs, botões e analisador lógico) devem estar no mesmo GND
  da ESP32. Em protoboards grandes, o trilho de GND costuma ser cortado no meio.
- Conferência rápida: com a placa desligada, use o multímetro em continuidade entre o GND da
  ESP32 e cada GND do circuito. Todos devem apitar.

Pinagem de referência para a bancada: `docs/bancada.md` (conforme `app_config.h`, verificado em 24/11/2026).

## 3. Decisão

**Decisão da equipe (25/11/2026): Opção B.** O aviso foi publicado em `simulação/README.md` e no
relatório técnico (seção 8.2, item 7). A Opção A fica como trabalho futuro.

As duas opções avaliadas foram:

**Opção A — recomendada:** o Wokwi passa a usar o núcleo verificado.
1. Copiar `core/fsm_core.c`, `core/fsm_core.h` e `core/fsm_config.h` para o projeto Wokwi, no
   lugar de `simulação/fsm_core.*`, **sem editar**.
2. Adaptar só o `sketch.ino` (o invólucro): montar `fsm_inputs_t` com `req1/req2/sig1_ok/sig2_ok/reset/dt_ms`,
   chamar `fsm_step()` e escrever os LEDs a partir de `fsm_outputs()`.
3. Ajustar `diagram.json` para a pinagem da bancada (tabela acima), incluindo dois interruptores
   (*slide switch*) nos GPIO 22/23 para simular a supervisão de linha (só necessário se `PTT7_SUPERVISAO_HW=1`).
4. Conferir o SHA-256 dos três arquivos contra `verification/results/versoes.txt`.

Custo estimado: algumas horas. Ganho: a demonstração no Wokwi passa a ser coberta pelas provas,
e o diagrama do Wokwi passa a servir de guia para a bancada.

**Opção B — mínima:** manter o Wokwi como está, mas declarar no relatório e no README da pasta
`simulação/` que ele é uma demonstração didática com núcleo próprio, **não verificado**, e não
usá-lo como evidência de P1–P6 nem de Q2/Q3.

Em qualquer opção, a evidência oficial de validação fica com: `core/` + testes + conformidade +
cenários do `sim/` + bancada com o firmware ESP-IDF.

## 4. Checklist de congelamento do modelo (antes do dia 26)

- [x] Conferir se `core/` mudou desde o núcleo verificado: `git log -1 --format=%h -- core/` continua `412b609`
  e os SHA-256 de `core/` coincidem com `verification/results/versoes.txt` (conferido em 25/11/2026).
- [x] Como `core/` não mudou, não foi preciso refazer a especificação nem o modelo; o ESBMC 8.5.0 foi reexecutado
  em 25/11/2026 (16 consultas, 0 refutadas/inconclusivas).
- [x] Commit e SHA-256 do núcleo registrados em `verification/results/versoes.txt` e no relatório.
- [x] Opção B aplicada (`simulação/README.md`).
- [ ] Conferir, antes do reteste, a bancada pela pinagem de `docs/bancada.md` (com `PTT7_SUPERVISAO_HW 0` não há jumpers em
  22/23; com `1`, jumpers ao GND) e o jumper de estímulo GPIO16 → GPIO33.
