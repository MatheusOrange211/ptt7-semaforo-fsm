# CVF - resultados da verificacao em duas camadas

- Modelo: Z3 5.4.1 (API C++) - 2026-11-21 11:28:47 - horizonte BMC k=10
- Codigo: ESBMC version 8.7.0 64-bit x86_64 linux - 2026-11-21 11:31:56

| Propriedade | Descricao | Modelo (Z3) | Codigo (ESBMC) | Concordancia |
|---|---|---|---|---|
| P1 | alcancabilidade dos estados | comprovado | comprovado | sim |
| P2 | ausencia de deadlock / transicao definida | comprovado | comprovado | sim |
| P3 | exclusao mutua dos verdes (+ verde minimo) | comprovado | comprovado | sim |
| P4 | violacao das entradas -> estado seguro | comprovado | comprovado | sim |
| P5 | convergencia do fallback | comprovado | inconclusivo | NAO |
| P6 | FALHA nao retorna direto a VP | comprovado | comprovado | sim |

| Camada | comprovado | valido_no_limite | refutado | inconclusivo |
|---|---|---|---|---|
| Modelo (Z3) | 6 (100%) | 0 (0%) | 0 (0%) | 0 (0%) |
| Codigo (ESBMC) | 5 (83%) | 0 (0%) | 0 (0%) | 1 (17%) |

CVF registra o numero e a proporcao das seis propriedades por categoria de resultado; nao representa cobertura de codigo nem de todos os comportamentos.
