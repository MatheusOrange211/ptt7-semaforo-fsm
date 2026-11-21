# Design review — perguntas prováveis e respostas preparadas

Use como treino antes do dia 24. As respostas são curtas para falar em voz alta; o detalhe está
no documento indicado entre parênteses.

## Sobre o desenho

**1. Por que uma máquina de estados e não um código com `if`s e `delay()`?**
Porque a FSM deixa explícito cada estado e cada mudança permitida. Isso permite enumerar tudo e
verificar formalmente. É exigência da trilha PTT7 (Catálogo: "FSM explícita e documentada").

**2. Por que 8 estados? PEDIDO_PEDESTRE não é redundante?**
Não. Ele representa "pedido registrado, mas o verde mínimo dos carros ainda não acabou". Sem ele,
um pedido feito no início do verde cortaria o verde dos carros, o que viola o requisito de verde
mínimo (P3b).

**3. O que acontece se os dois botões forem apertados ao mesmo tempo?**
Os dois pedidos viram uma única demanda (`pending`), porque há uma única travessia. O ciclo é o
mesmo. Isso está nos testes e no modelo.

**4. E se o botão for apertado durante o verde de pedestre?**
O pedido é ignorado, porque a travessia já está sendo atendida. Em FALHA também é ignorado.
Nos outros estados, o pedido fica guardado e é atendido no próximo ciclo.

**5. Como o sistema sabe que um botão "perdeu o sinal"?**
Por supervisão de linha: cada botão tem um fio de supervisão (GPIO 22/23) que deve estar no GND.
Se ele se solta, o sinal é considerado perdido e a FSM vai para FALHA. (`docs/bancada.md`)

**6. O que é a "falha de temporização"?**
Se a tarefa de controle demorar mais de 200 ms entre dois passos (`DT_MAX`), o núcleo entende que
algo travou e vai para FALHA. O tempo decorrido é sempre uma entrada explícita do núcleo.

## Sobre a verificação

**7. Por que usar Z3 e ESBMC, e não só um deles?**
O Z3 verifica o modelo abstrato, com inteiros matemáticos, e é bom para raciocinar sobre a lógica.
O ESBMC verifica o código C real, com inteiros de 32 bits. Ele encontrou um *overflow* que o Z3
não tinha como ver (achado ESBMC-01). Cada camada pega erros diferentes.

**8. Como garantem que o código no ESP32 é o mesmo que foi verificado?**
O mesmo arquivo `core/fsm_core.c` é compilado no firmware (componente `fsm_core`) e analisado pelo
ESBMC, sem cópia nem reescrita. O commit e o SHA-256 do núcleo verificado estão registrados em
`verification/results/versoes.txt`. *Ressalva honesta:* a simulação no Wokwi ainda usa um núcleo
próprio; ou a alinhamos, ou ela fica declarada como demonstração não verificada
(`docs/validacao/alinhamento_modelo.md`).

**9. E como garantem que o modelo Z3 corresponde ao código C?**
Por conformidade: os mesmos vetores de entrada aleatórios (semente 2026) são aplicados ao C e ao
modelo, comparando estado, transição, temporizadores e saídas a cada passo. Resultado: 16/16
transições cobertas e 0 divergências. Não é prova de equivalência, e o contrato não promete isso
(item 6.2).

**10. O contrato fala em limite de 10 passos. Isso basta?**
Para alcançabilidade, não. Com passos de até 200 ms, um ciclo completo leva 133 passos. Revisamos
o limite, como o próprio protocolo prevê: o Z3 usa 133 passos, e o ESBMC faz a alcançabilidade
em 8 segmentos. As propriedades de segurança são provadas em 1 passo sobre qualquer contexto
válido (argumento indutivo), o que vale para execuções de qualquer tamanho. (`docs/registro_verificacao.md`)

**11. Como sabem que a verificação não está "passando tudo" por engano?**
Pelo teste negativo: um mutante que pula RECOVERY (`-DFSM_INJECT_BUG`) é refutado nas duas
ferramentas, com contraexemplo.

**12. Algum resultado foi inconclusivo?**
Com o ESBMC 8.5.0, nenhum. Na reexecução de 21/11 testamos também o 8.7.0: a k-indução de P5
ficou inconclusiva, sem contraexemplo. P5 continua sustentada pelo passo indutivo no Z3, pelo BMC
k = 10 e pelas provas de P4 e P6. Fixamos a versão 8.5.0 e registramos isso como limitação.

## Sobre a validação e a bancada

**13. O que é "validar a máquina" para vocês?**
Mostrar que cada transição especificada acontece de fato no que roda. A matriz de rastreabilidade
liga T1–T16 e P1–P6 a teste unitário, conformidade, cenário de aceitação e teste de bancada.

**14. Como medem a latência?**
Com `esp_timer_get_time()`, da entrada da ISR do botão até o GPIO atualizado: é latência interna
de software. Os logs saem por fila numa tarefa de baixa prioridade, fora do trecho medido. São 30
medições válidas em 3 sessões, com 5 de aquecimento por sessão. Sem analisador lógico, não
afirmamos latência física de ponta a ponta.

**15. Como testam as falhas?**
São 50 injeções por tipo: perda do sinal 1, perda do sinal 2, perda simultânea durante o verde de
pedestre e, como extra, falha de temporização. O sucesso exige verdes apagados e FALHA em até
1 s, sem voltar ao verde.

**16. E se a placa não funcionar a tempo?**
Plano B do contrato: os mesmos experimentos no simulador de host com o mesmo núcleo, declarando
a ausência de validação física e sem atribuir tempos ao ESP32. Os dados do Plano B já foram gerados.

## Perguntas difíceis (seja honesto)

- **"Os dados de bancada ainda não existem?"** Ainda não. A montagem está em ajuste (pinagem e
  GND), e o protocolo e os scripts já estão prontos para rodar.
- **"Isso garante segurança de um semáforo real?"** Não. É um protótipo didático; certificação e
  tempo real duro estão fora do escopo (contrato, item 6.2).
