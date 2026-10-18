# ADR-003: Persistência com SQLite e arquivos no sistema de arquivos

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

O AstroCapture precisa armazenar sessões, sequências, configurações, metadados, histórico de operações e imagens capturadas.

Arquivos RAW podem ser grandes e precisam permanecer acessíveis independentemente dos registros de metadados.

## Decisão

Utilizar SQLite para persistir os metadados e o sistema de arquivos para armazenar as imagens.

O banco será a fonte principal de verdade para sessões, sequências e quadros. Um arquivo `session.json` poderá ser utilizado como manifesto complementar, sem substituir o banco.

## Justificativa

- SQLite oferece transações e persistência local sem exigir um servidor de banco de dados.
- O sistema de arquivos é adequado para armazenar imagens de grande volume.
- A separação facilita backup, inspeção dos arquivos e evolução do processamento.

## Consequências

- O banco deverá registrar o caminho de cada arquivo.
- A gravação de uma imagem e a confirmação dos metadados deverão seguir uma sequência segura.
- A aplicação deverá detectar inconsistências entre banco e arquivos durante a recuperação.
- A exclusão de arquivos não deverá ocorrer silenciosamente.
- Migrações de esquema deverão ser versionadas.
- O backup manual da pasta de dados deverá ser documentado.

O SQLite e o sistema de arquivos não compartilham uma transação atômica. A implementação deverá tratar explicitamente falhas ocorridas entre a gravação do arquivo e a confirmação dos metadados.