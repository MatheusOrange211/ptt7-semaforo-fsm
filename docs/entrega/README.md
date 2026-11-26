# Materiais de entrega — PTT7

Organizados a partir dos dois documentos-base do projeto: o **Catálogo PTT7** (escopo mínimo e
artefatos da trilha) e o **contrato assinado** (projeto de pesquisa de 08/09/2026, em especial os
itens 6.3 e 10.1). Situação em 25/11/2026: verificação e Q3 concluídas; reteste da Q2 em execução.

## Entrega final (26/11/2026)

| Arquivo | Para quê |
|---|---|
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
| Dados brutos de latência e injeção de falhas + registro de descartes | `data/raw/`, `data/descartes.csv` | ⚠️ Q3 completo (200/200, ESP32, 24/11); Q2 em reteste (`reteste_s*_lat.log`). Plano B fica como referência |
| Vídeo de demonstração (3–5 min) | roteiro em `final/roteiro_video.md` | ⏳ gravar |
| Relatório técnico conclusivo | `final/relatorio_tecnico.md` | ✅ formato de artigo; Q2 será atualizada após o reteste |

## Artefatos do Catálogo PTT7

| Artefato | Situação |
|---|---|
| Firmware | ✅ `firmware/` + `core/` |
| Modelo formal da FSM | ✅ `verification/z3/fsm_model.hpp` + `spec/` |
| Saída da verificação | ✅ `verification/results/` |
| Evidência de teste em bancada | ⚠️ Q3 (200/200) e 11 de 16 transições registradas nos logs de 24/11; Q2 em reteste; F1–F10 não executado como roteiro (ver matriz de rastreabilidade) |
| Relatório | ✅ `final/relatorio_tecnico.md` |

## O que falta, em ordem

1. Refazer a Q2 conforme o protocolo, pelo roteiro único do `MANUAL_EXECUCAO.md`, seção 8.1, e
   depois rodar `python analysis/analyze.py`.
2. Atualizar com os novos números o relatório (seções 7.4 e 9 e Apêndice A), o `README.md` e a
   matriz de rastreabilidade.
3. Gravar o vídeo de demonstração.
4. Se `core/` mudar, rodar `python3 tools/ptt7.py all` de novo (com ESBMC 8.5.0).
