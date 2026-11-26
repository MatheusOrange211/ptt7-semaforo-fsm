# Materiais de entrega — PTT7

Organizados a partir dos dois documentos-base do projeto: o **Catálogo PTT7** (escopo mínimo e
artefatos da trilha) e o **contrato assinado** (projeto de pesquisa de 08/09/2026, em especial os
itens 6.3 e 10.1), disponível em [`contrato/projeto_pesquisa_ptt7_assinado.pdf`](contrato/projeto_pesquisa_ptt7_assinado.pdf). Situação em 26/11/2026: verificação e Q3 concluídas; Q2 remedida (30 válidas, protocolo parcialmente atendido).

## Entrega final (26/11/2026)

| Arquivo | Para quê |
|---|---|
| `contrato/projeto_pesquisa_ptt7_assinado.pdf` | contrato (projeto de pesquisa) assinado digitalmente pelos dois discentes (gov.br e SerproID) |
| `final/relatorio_tecnico.md` | relatório técnico conclusivo (produto secundário), em formato de artigo |
| `final/relatorio_tecnico.pdf` | o mesmo relatório em PDF (gerado a partir do `.md`) |
| `final/slides_apresentacao_final.pptx` | apresentação final |
| `final/roteiro_video.md` | roteiro do vídeo de 3–5 min |
| `../validacao/matriz_rastreabilidade.md` | validação da máquina: T1–T16 e P1–P6 × evidências |
| `../validacao/alinhamento_modelo.md` | divergências núcleo verificado × Wokwi × bancada (decisão: Opção B) |

## Checklist dos entregáveis-base (contrato, item 10.1)

| Entregável | Onde | Situação |
|---|---|---|
| Repositório versionado com licença, firmware, `fsm_core.c`, modelo formal, tabela de transições e procedimentos de verificação | raiz do repositório | ✅ |
| README de reprodutibilidade (bancada, ferramentas, versões, comandos, premissas, limites) | `README.md`, `MANUAL_EXECUCAO.md`, `docs/bancada.md`, `verification/results/versoes.txt` | ✅ (ESBMC fixado na tag `v8.5` com SHA-256; revisado em 25/11) |
| Saídas do Z3/ESBMC e scripts de análise | `verification/results/`, `analysis/` | ✅ (reexecução 21/11; conferência em clone limpo 25/11) |
| Dados brutos de latência e injeção de falhas + registro de descartes | `data/raw/`, `data/descartes.csv` | ⚠️ Q3 completo (200/200, ESP32, 24/11); Q2: 30 medições válidas em `reteste_s1..s6_lat.log` (26/11), protocolo parcialmente atendido. Plano B fica como referência |
| Vídeo de demonstração (3–5 min) | roteiro em `final/roteiro_video.md` | ⏳ gravar |
| Relatório técnico conclusivo | `final/relatorio_tecnico.md` | ✅ formato de artigo; Q2 atualizada com o reteste de 26/11 (vídeos de execução no Apêndice B) |

## Artefatos do Catálogo PTT7

| Artefato | Situação |
|---|---|
| Firmware | ✅ `firmware/` + `core/` |
| Modelo formal da FSM | ✅ `verification/z3/fsm_model.hpp` + `spec/` |
| Saída da verificação | ✅ `verification/results/` |
| Evidência de teste em bancada | ⚠️ Q3 (200/200) e 11 de 16 transições registradas nos logs de 24/11; Q2 com 30 válidas (26/11, parcial); F1–F10 não executado como roteiro (ver matriz de rastreabilidade) |
| Relatório | ✅ `final/relatorio_tecnico.md` |

## O que falta, em ordem

1. Gravar o vídeo de demonstração.
2. (Opcional, para atender a Q2 por completo) repetir a Q2 em 3 sessões com `sessao N` e reinício
   da placa, pelo `MANUAL_EXECUCAO.md`, seção 8.1.
4. Se `core/` mudar, rodar `python3 tools/ptt7.py all` de novo (com ESBMC 8.5.0).
