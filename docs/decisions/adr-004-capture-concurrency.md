# ADR-004: Execução assíncrona do fluxo de captura

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

Exposições astronômicas podem durar vários segundos ou minutos. Transferências, gravações e validações também podem demorar.

Executar essas operações na thread principal prejudicaria a responsividade da interface.

## Decisão

Executar o fluxo de captura em uma thread de trabalho separada da interface Qt.

O acesso à câmera deverá ser serializado para evitar chamadas concorrentes ao mesmo dispositivo.

A sequência será controlada por uma máquina de estados explícita:

- `Ready`
- `Running`
- `StopRequested`
- `Paused`
- `Completed`
- `Failed`

Cada quadro terá seu próprio estado de processamento.

## Justificativa

A execução assíncrona mantém a interface responsiva e torna o comportamento de parada, falhas e recuperação mais previsível.

## Consequências

- A interface deverá receber atualizações por mecanismos seguros de comunicação entre threads.
- A thread principal não deverá executar exposições ou transferências demoradas.
- O acesso à câmera deverá ter um único responsável por vez.
- A parada de uma sequência contínua deverá impedir novos disparos, mas permitir a conclusão segura do quadro atual.
- Um quadro só será concluído após transferência, validação, gravação e persistência dos metadados.
- O encerramento da aplicação deverá considerar operações em andamento.