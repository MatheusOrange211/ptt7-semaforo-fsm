# Alinhamento do modelo da máquina — núcleo verificado × simulação Wokwi × bancada

Situação em 21/11/2026, com o repositório no commit `b4fcf0e` (GitHub e cópia local iguais).

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
| LED pedestre VM / VD | **32 / 33** | **18 / 19** |
| Botão pedestre 1 / 2 | **18 / 19** | **32 / 33** |
| Botão reset | **21** | **23** |
| Supervisão do sinal 1 / 2 (jumper ao **GND**) | **22 / 23** | não existe |
| Estímulo controlado (Q2) | 16 → 18, 17 → 19 | não existe |

**Atenção para a bancada (possível causa do "problema do GND"):**
- Se a protoboard foi montada pelo desenho do Wokwi e gravada com o firmware ESP-IDF, os LEDs de
  pedestre ficam nos pinos que o firmware lê como botões, e vice-versa.
- No firmware, os GPIO **22 e 23 precisam de jumper ligado ao GND**. Sem ele, o núcleo entende
  "perda de sinal do botão" e vai para FALHA (amarelo piscante) e não sai, nem com reset. No
  Wokwi, o GPIO 23 é o botão de reset; ligado ao GND o tempo todo, ele mantém o reset pressionado.
- Todos os GNDs (cátodos dos LEDs, botões, jumpers e analisador lógico) devem estar no mesmo GND
  da ESP32. Em protoboards grandes, o trilho de GND costuma ser cortado no meio.
- Conferência rápida: com a placa desligada, use o multímetro em continuidade entre o GND da
  ESP32 e cada GND do circuito. Todos devem apitar.

Pinagem de referência para a bancada: `docs/bancada.md` (igual a `app_config.h`).

## 3. Decisão proposta (para a equipe confirmar)

**Opção A — recomendada:** o Wokwi passa a usar o núcleo verificado.
1. Copiar `core/fsm_core.c`, `core/fsm_core.h` e `core/fsm_config.h` para o projeto Wokwi, no
   lugar de `simulação/fsm_core.*`, **sem editar**.
2. Adaptar só o `sketch.ino` (o invólucro): montar `fsm_inputs_t` com `req1/req2/sig1_ok/sig2_ok/reset/dt_ms`,
   chamar `fsm_step()` e escrever os LEDs a partir de `fsm_outputs()`.
3. Ajustar `diagram.json` para a pinagem da bancada (tabela acima), incluindo dois interruptores
   (*slide switch*) nos GPIO 22/23 para simular a supervisão de linha.
4. Conferir o SHA-256 dos três arquivos contra `verification/results/versoes.txt`.

Custo estimado: algumas horas. Ganho: a demonstração no Wokwi passa a ser coberta pelas provas,
e o diagrama do Wokwi passa a servir de guia para a bancada.

**Opção B — mínima:** manter o Wokwi como está, mas declarar no relatório e no README da pasta
`simulação/` que ele é uma demonstração didática com núcleo próprio, **não verificado**, e não
usá-lo como evidência de P1–P6 nem de Q2/Q3.

Em qualquer opção, a evidência oficial de validação fica com: `core/` + testes + conformidade +
cenários do `sim/` + bancada com o firmware ESP-IDF.

## 4. Checklist de congelamento do modelo (antes do dia 26)

- [ ] Obter a versão atual do firmware e conferir se `core/` mudou (`git diff 412b609 -- core/`).
- [ ] Se mudou: atualizar `spec/transition_table.csv` e o modelo Z3 juntos e rodar `python3 tools/ptt7.py all`.
- [ ] Registrar commit e SHA-256 do núcleo em `verification/results/versoes.txt` e no relatório.
- [ ] Aplicar a Opção A ou B acima.
- [ ] Bancada montada pela pinagem de `docs/bancada.md`, com os jumpers dos GPIO 22/23 no GND.
