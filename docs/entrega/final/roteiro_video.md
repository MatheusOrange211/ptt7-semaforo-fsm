# Roteiro do vídeo de demonstração (3–5 min)

**Exigência do contrato (item 10.1):** vídeo de 3–5 min com *operação normal*, *resposta à demanda
de travessia* e *recuperação após falha*.

**Gravação:** celular fixo filmando a protoboard, com o monitor serial visível no notebook ao lado
(ou gravação de tela em janela dividida). Cada cena abaixo já indica o item do teste funcional
(MANUAL_EXECUCAO.md, seção 7.3) que ela reaproveita, de modo que a mesma gravação serve de
evidência de bancada.

> **Plano B:** se a placa não estiver pronta, gravar o simulador de host (`python3 tools/ptt7.py sim`),
> que usa o núcleo verificado, e dizer em voz alta que é simulação. **Não usar o Wokwi** (`simulação/`)
> como demonstração do núcleo verificado: ele tem uma FSM própria, não verificada (ver `simulação/README.md`).

| # | Tempo | Cena (imagem) | Fala (resumo) |
|---|---|---|---|
| 1 | 0:00–0:25 | Título e protoboard em plano geral | "PTT7: semáforo com travessia de pedestres no ESP32, controlado por uma máquina de estados verificada em duas camadas." |
| 2 | 0:25–0:55 | Diagrama de estados (8 estados) | "São 8 estados e 16 transições. A lógica está num único arquivo C, sem hardware, e esse mesmo arquivo vai para o ESP32 e para o verificador." |
| 3 | 0:55–1:25 | **Operação normal** (F1): ligar a placa; vermelho total por 2 s e depois verde dos carros | "Ao ligar, a FSM sai de INIT para VERDE_VEICULO, transição T1." Mostrar a linha `TR ... T1` na serial. |
| 4 | 1:25–2:15 | **Demanda de travessia** (F2): apertar o botão de pedestre; amarelo 3 s, verde de pedestre 6 s, vermelho piscante 3 s e volta ao verde | "Pedido com o verde mínimo cumprido: amarelo, travessia e volta. Em nenhum momento os dois verdes acendem juntos — essa é a propriedade P3, provada no modelo e no código." |
| 5 | 2:15–2:45 | **Verde mínimo** (F3): apertar logo após o verde começar | "Pedido antecipado: o sistema registra, mas espera os 8 s de verde dos carros (P3b)." |
| 6 | 2:45–3:25 | **Falha** (F4–F5): digitar `perda 1` no monitor serial; amarelo piscante; apertar RESET com a perda ativa, sem efeito | "Perda do sinal do botão: tudo vai para FALHA, amarelo piscante (P4). Com a falha presente, o reset é ignorado." |
| 7 | 3:25–4:05 | **Recuperação** (F6): digitar `restaura` e apertar RESET; vermelho total por 3 s e depois verde | "Sem falha, o reset leva a RECOVERY e só depois ao verde. O sistema nunca volta direto ao verde de pedestre (P6)." |
| 8 | 4:05–4:40 | Tela com a tabela CVF (6/6 nas duas camadas) e um gráfico de latência/fallback | "Resultados: as seis propriedades foram comprovadas no Z3 e no ESBMC. Na bancada, o *fallback* chegou ao estado seguro em 200 de 200 falhas injetadas." Acrescentar a latência mediana da Q2 depois do reteste (`analysis/analyze.py`). |
| 9 | 4:40–5:00 | Encerramento | Matheus Naranjo Corrêa e Pedro Vinicius da Silva Ribeiro, orientador Prof. Herbert Rocha, e o link do repositório. |

## Checklist antes de gravar

- [ ] Firmware gravado com o núcleo do commit registrado em `verification/results/versoes.txt`.
- [ ] Com `PTT7_SUPERVISAO_HW=0` (padrão) **não** há jumpers em GPIO22/23; a perda de sinal é digitada (`perda 1`, `restaura`). Só com HW=1 os jumpers ao GND são necessários (sem eles a placa começa em FALHA).
- [ ] Monitor serial aberto e legível (fonte grande).
- [ ] Ensaiar as cenas 3–7 uma vez antes; cada ciclo completo leva cerca de 20 s.
- [ ] Acrescentar à cena 8 a latência da Q2 depois do reteste (`analysis/analyze.py`).
- [ ] Duração final entre 3 e 5 min.
