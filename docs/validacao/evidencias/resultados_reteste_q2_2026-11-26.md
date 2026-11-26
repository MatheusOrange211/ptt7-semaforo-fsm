# Reteste da Q2 (latência ISR → GPIO) — bancada de 26/11/2026

**Situação: protocolo PARCIALMENTE atendido.** 30 medições válidas de T4 (meta: ≥ 30), todas com
LREC de 20 µs, mas em seis capturas de um mesmo *boot* e com um único rótulo de sessão (meta: ≥ 3
sessões). O veredito formal do `analyze.py` continua "NÃO atendido (30 válidas, 1 sessão)".

## Como foi coletado

- Placa, firmware, cabo e computador iguais aos de 24/11/2026 (o código-fonte de `core/` e
  `firmware/main/` e o `sdkconfig` da cópia da equipe são idênticos aos do repositório).
- Seis capturas com `capture_serial.py` e o comando `lat 5 1` (1 ensaio de aquecimento + 5 válidos),
  iniciadas às 10:19, 10:22, 10:26, 10:39, 10:46 e 10:49.
- A equipe gravou os arquivos como `s1_lat.log` … `s6_lat.log`. Na integração eles foram
  **renomeados** para `reteste_s1_lat.log` … `reteste_s6_lat.log`, sem alterar o conteúdo
  (conferido com `cmp`), para não sobrescrever o `s1_lat.log` bruto de 24/11.
- Vídeos de execução (bateria de testes e sessão de latência): <https://drive.google.com/drive/folders/1w-mvAM3hBnI3b9LSmzt6A5wP4t3ufybH?usp=sharing>.

| Arquivo bruto | SHA-256 |
|---|---|
| `reteste_s1_lat.log` | `476544184e509df24e3a4022cde1db013a01244a5e39ea7f9c88a23ea029584d` |
| `reteste_s2_lat.log` | `6fcdb0d51d868f52318dc5291d8a9ca1f503c5c1bf9575d54d91aa0b5b0c3f81` |
| `reteste_s3_lat.log` | `43ac86ae9b4c81e6c2e5696743e4c8729b4a64b723c7939740d3bac3c08c7ac7` |
| `reteste_s4_lat.log` | `8348462841ab5026865371cf0e315940762cfc54d631b621d004824924bc43a3` |
| `reteste_s5_lat.log` | `17778a01409b8394b71161bc7c1cafab4c1e0bfefb11f9210b91708fb1f51b0d` |
| `reteste_s6_lat.log` | `28d7097647c55d57d1b6dc461e3fa87aae9e617365487a3868eeace0c14c6f95` |

## Resultado

| Captura | *uptime* no 1º válido (s) | n | média | mediana | DP | mín | máx |
|---|---|---|---|---|---|---|---|
| `reteste_s1_lat.log` | 741,3 | 5 | 20,0 | 20,0 | 0,0 | 20 | 20 |
| `reteste_s2_lat.log` | 926,7 | 5 | 20,0 | 20,0 | 0,0 | 20 | 20 |
| `reteste_s3_lat.log` | 1199,3 | 5 | 20,0 | 20,0 | 0,0 | 20 | 20 |
| `reteste_s4_lat.log` | 1973,1 | 5 | 20,0 | 20,0 | 0,0 | 20 | 20 |
| `reteste_s5_lat.log` | 2380,6 | 5 | 20,0 | 20,0 | 0,0 | 20 | 20 |
| `reteste_s6_lat.log` | 2567,6 | 5 | 20,0 | 20,0 | 0,0 | 20 | 20 |
| **total** | — | **30** | **20,0** | **20,0** | **0,0** | **20** | **20** |

- Latência até a decisão da FSM (antes de escrever os GPIOs): 18 µs nas 30.
- Aquecimento (excluído): 6 registros, 20 µs (5) e 19 µs (1).
- 0 descartes, 0 `#AVISO`, 0 acionamentos não comandados; as seis capturas terminam com `#FIM lat`.
- As capturas também registram 36 ciclos completos de travessia: amarelo 3000,4 ms, verde de pedestre
  6000,0 ms e atenção de pedestre 3000,0 ms, sem variação (Tabela 7 do relatório).

## Desvios em relação ao roteiro (MANUAL, seção 8.1)

1. **`sessao N` não enviado:** todos os registros saíram com sessão 1.
2. **Sem reinício da placa entre as capturas:** o *uptime* cresce sem interrupção de uma captura
   para a outra, de modo que as capturas são janelas de um mesmo *boot*, e não sessões independentes.
3. **`lat 5 1` em vez de `lat 10 5`:** 6 ensaios de aquecimento no total, contra 15.
4. **Sem `reteste_sN_boot.log`:** a *build* gravada não foi registrada.
5. A primeira captura começa com 115 fragmentos de linha repetidos (`RDE_VEICULO,T12`), recebidos
   **antes** do comando `lat`. Eles não contêm registros LAT e não afetam a medição. O arquivo foi
   mantido bruto.

## Observações

- **DP = 0:** todos os instantes de entrada da ISR terminam em …882 µs. O atraso aleatório antes do
  pulso é sorteado em milissegundos inteiros, então o estímulo cai sempre na mesma fase do *tick* de
  1 ms do FreeRTOS. O caminho é determinístico nessa condição, mas outras fases e o pior caso não
  foram exploradas.
- **20 µs agora contra 13–14 µs em 24/11:** o código e a configuração são idênticos e a causa não foi
  identificada. Sem o *banner*, não dá para confirmar que a mesma *build* estava gravada.

## Regra de análise acrescentada depois de ver os dados

O `analysis/analyze.py` passou a mostrar uma tabela descritiva **por captura** (com o *uptime*) e a
linha "Situação: PARCIALMENTE atendido" quando há ≥ 30 válidas em ≥ 3 capturas, mas menos de 3
rótulos de sessão. A regra formal (≥ 30 válidas em ≥ 3 sessões) não mudou.

Saída do `analyze.py`: `reteste_2026-11-26/resumo.md`, `latencias.csv` e `latencia_boxplot.png`.
