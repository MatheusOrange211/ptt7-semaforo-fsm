# Saída original de `analysis/out/` — bancada de 24/11/2026

Estes quatro arquivos (`resumo.md`, `falhas.csv`, `latencias.csv`, `recuperacao.csv`) são a saída do
`analysis/analyze.py` gerada pela equipe logo após o ensaio, **sem nenhuma alteração**. Foram copiados para cá
porque `analysis/out/` não é versionado (`.gitignore`).

Atenção ao ler o `resumo.md` desta pasta: a tabela de Q2 dele conta como "válidos" 17 registros LAT
incidentais (toques de botão durante os ensaios de falha), que não pertencem ao experimento `lat`. A versão
atual do `analyze.py` separa esses registros. A leitura consolidada está em
`../../resultados_bancada_2026-11-24.md`. Os números de Q3 são idênticos nas duas versões.
