# Simulação no Wokwi — demonstração didática (núcleo NÃO verificado)

> **Atenção.** Esta pasta tem uma versão **própria e reescrita** da máquina de estados
> (`simulação/fsm_core.c` e `fsm_core.h`). Ela **não** é o núcleo verificado do projeto
> (`core/fsm_core.c`). Por isso:
>
> - as provas P1–P6 (Z3 e ESBMC) **não** se aplicam a esta simulação;
> - esta simulação **não** é usada como evidência de P1–P6, da Q2 (latência) nem da Q3 (*fallback*);
> - ela não deve ser apresentada, no vídeo ou no relatório, como demonstração do núcleo verificado.

## Por que ela é diferente

A simulação foi feita numa etapa inicial do projeto, antes da especificação final. Ela difere do
núcleo verificado em 9 pontos (tempos menores, limite de atraso de 250 ms em vez de 200 ms, saída da
recuperação para INIT, *reset* aceito em qualquer estado, entre outros) e usa pinagem de botões
diferente da bancada. A lista completa, com o efeito de cada diferença, está em
[`docs/validacao/alinhamento_modelo.md`](../docs/validacao/alinhamento_modelo.md).

Exemplo simples: é como provar que uma receita está certa e depois cozinhar outra parecida. O bolo
pode até sair bom, mas a prova não fala sobre ele.

## Onde está a evidência oficial

- Núcleo verificado: `core/` (mesmo arquivo compilado no firmware e analisado pelo ESBMC).
- Verificação, testes e cenários: `verification/`, `tests/`, `sim/` (simulador de computador que usa
  o núcleo verificado).
- Bancada física: firmware ESP-IDF em `firmware/` e logs em `data/raw/`.

## Arquivos desta pasta

| Arquivo | Conteúdo |
|---|---|
| `sketch.ino` | integração com os LEDs no Wokwi e bateria automática de 7 cenários |
| `diagram.json` | componentes e ligações do circuito no Wokwi |
| `fsm_core.c`, `fsm_core.h` | FSM própria da simulação (não verificada) |
| `especificacao.md` | descrição original da simulação e de como executá-la |
| `resultados_simulacao.txt` | registro da rodada de cenários da simulação |
