# Design review — checklist para avaliar o projeto da outra equipe

O design review é uma revisão por pares: cada equipe olha o **desenho** da outra procurando
lacunas, como dois mecânicos que trocam de carro para inspecionar. O objetivo é ajudar, não
reprovar. Para cada item, marque ✅ (atende), ⚠️ (parcial) ou ❌ (não atende), anote uma
observação e, quando possível, uma sugestão concreta.

Equipe avaliada: ______________________ · Trilha PTT: ______ · Avaliadores: ______________________

## 1. Alinhamento com a trilha e o contrato

| # | Pergunta | Nota | Observação / sugestão |
|---|---|---|---|
| 1.1 | O produto atende ao **escopo mínimo** da trilha no Catálogo? | | |
| 1.2 | Os **artefatos** exigidos pela trilha estão previstos (e onde estão)? | | |
| 1.3 | O que está **fora do escopo** está declarado e respeitado? | | |
| 1.4 | O problema de pesquisa e os objetivos são claros e verificáveis? | | |

## 2. Desenho técnico

| # | Pergunta | Nota | Observação / sugestão |
|---|---|---|---|
| 2.1 | A arquitetura separa as responsabilidades (lógica × hardware × instrumentação)? | | |
| 2.2 | As decisões principais estão justificadas (por que esta plataforma, ferramenta ou algoritmo)? | | |
| 2.3 | Há uma especificação única que o código e o modelo/testes seguem? | | |
| 2.4 | As interfaces e entradas/saídas estão definidas (pinos, sinais, formatos)? | | |
| 2.5 | Casos de borda e falhas foram pensados (entradas inválidas, tempo, reset)? | | |

## 3. Evidências e avaliação

| # | Pergunta | Nota | Observação / sugestão |
|---|---|---|---|
| 3.1 | As métricas estão definidas com unidade e forma de medir? | | |
| 3.2 | O protocolo tem repetições, dispersão e critério de descarte? | | |
| 3.3 | Os resultados são reproduzíveis (README, versões, comandos)? | | |
| 3.4 | As limitações e ameaças à validade estão declaradas? | | |
| 3.5 | Existe ligação clara entre requisito → teste/verificação → evidência? | | |

## 4. Viabilidade

| # | Pergunta | Nota | Observação / sugestão |
|---|---|---|---|
| 4.1 | O que falta cabe no prazo? | | |
| 4.2 | Os riscos principais têm plano B concreto? | | |
| 4.3 | Há dependência de equipamento ou pessoa que pode travar a entrega? | | |

## 5. Síntese para entregar à equipe

- **Três pontos fortes:**
  1.
  2.
  3.
- **Três riscos ou lacunas mais importantes:**
  1.
  2.
  3.
- **Sugestão prioritária (a mais útil para eles até a entrega):**

## Dicas de postura

- Pergunte "como vocês sabem que…?" em vez de afirmar "está errado".
- Separe o que **falta fazer** (prazo) do que está **mal desenhado** (risco).
- Toda crítica deve vir com uma sugestão concreta.
