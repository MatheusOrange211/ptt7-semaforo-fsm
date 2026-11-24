# Materiais de entrega — PTT7

Organizados a partir dos dois documentos-base do projeto: o **Catálogo PTT7** (escopo mínimo e
artefatos da trilha) e o **contrato assinado** (projeto de pesquisa de 08/09/2026, em especial os
itens 6.3 e 10.1). Situação em 24/11/2026 (bancada física realizada).

## Design review (24/11/2026)

| Arquivo | Para quê |
|---|---|
| `design_review/resumo_projeto.md` | resumo de 1–2 páginas para entregar ou projetar |
| `design_review/slides_design_review.pptx` | apresentação curta (8 slides) |
| `design_review/perguntas_respostas.md` | treino: perguntas prováveis da outra equipe, com respostas |
| `design_review/checklist_avaliacao_outra_equipe.md` | roteiro para a nossa avaliação da outra equipe |

## Entrega final (26/11/2026)

| Arquivo | Para quê |
|---|---|
| `final/relatorio_tecnico.md` | relatório técnico conclusivo (produto secundário); preenchido com os dados de bancada de 24/11/2026 |
| `final/slides_apresentacao_final.pptx` | apresentação final |
| `final/roteiro_video.md` | roteiro do vídeo de 3–5 min |
| `../validacao/matriz_rastreabilidade.md` | validação da máquina: T1–T16 e P1–P6 × evidências |
| `../validacao/alinhamento_modelo.md` | divergências núcleo verificado × Wokwi × bancada, e a decisão a tomar |

## Checklist dos entregáveis-base (contrato, item 10.1)

| Entregável | Onde | Situação |
|---|---|---|
| Repositório versionado com licença, firmware, `fsm_core.c`, modelo formal, tabela de transições e procedimentos de verificação | raiz do repositório | ✅ |
| README de reprodutibilidade (bancada, ferramentas, versões, comandos, premissas, limites) | `README.md`, `MANUAL_EXECUCAO.md`, `docs/bancada.md`, `verification/results/versoes.txt` | ✅ (versões registradas em 21/11) |
| Saídas do Z3/ESBMC e scripts de análise | `verification/results/`, `analysis/` | ✅ (reexecução 21/11) |
| Dados brutos de latência e injeção de falhas + registro de descartes | `data/raw/`, `data/descartes.csv` | ⚠️ Q3 completo (200/200, ESP32, 24/11); Q2 só exploratório (protocolo não atendido). Plano B fica como referência |
| Vídeo de demonstração (3–5 min) | roteiro em `final/roteiro_video.md` | ⏳ gravar |
| Relatório técnico conclusivo | `final/relatorio_tecnico.md` | ✅ preenchido com os dados de bancada de 24/11/2026 (Q2 exploratório) |

## Artefatos do Catálogo PTT7

| Artefato | Situação |
|---|---|
| Firmware | ✅ `firmware/` + `core/` |
| Modelo formal da FSM | ✅ `verification/z3/fsm_model.hpp` + `spec/` |
| Saída da verificação | ✅ `verification/results/` |
| Evidência de teste em bancada | ⚠️ Q3 (200/200) e Q2 exploratório coletados em 24/11; F1–F10 e B1–B5 não executados (ver matriz de rastreabilidade) |
| Relatório | ✅ `final/relatorio_tecnico.md` |

## O que falta, em ordem

1. Refazer a Q2 conforme o protocolo: jumpers GPIO16 → GPIO33 e GPIO17 → GPIO32, fechar o
   `idf.py monitor` e rodar 3 sessões (comandos em `final/relatorio_tecnico.md`, seção 9), depois
   `python analysis/analyze.py`.
2. Os itens B1–B5 da matriz foram escritos para os jumpers dos GPIO 22/23, que ficam ignorados com
   `PTT7_SUPERVISAO_HW = 0` (bancada de 24/11): religar `HW = 1` ou aceitar a injeção por software.
3. Decidir a Opção A ou B do alinhamento do Wokwi (`../validacao/alinhamento_modelo.md`).
4. Se o código novo mudar `core/`, rodar `python3 tools/ptt7.py all` de novo (com ESBMC 8.5.0).
5. Atualizar a cena 8 do vídeo com os resultados reais (Q3 200/200; Q2 exploratório) e gravar o vídeo.
