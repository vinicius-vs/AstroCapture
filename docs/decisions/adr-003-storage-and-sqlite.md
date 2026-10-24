# ADR-003: Persistência com SQLite e arquivos no sistema de arquivos

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

O AstroCapture precisa armazenar sessões, sequências, configurações, metadados, histórico de operações e imagens capturadas.

Arquivos RAW podem ser grandes e precisam permanecer acessíveis independentemente dos registros de metadados.

## Decisão

Utilizar SQLite para persistir os metadados e o sistema de arquivos para armazenar as imagens.

O banco será a fonte principal de verdade para sessões, sequências e quadros. Um arquivo `session.json` poderá ser utilizado como manifesto complementar, sem substituir o banco.

O acesso ao SQLite será realizado pelo módulo Qt SQL com o driver `QSQLITE`. Será utilizado um banco único, chamado `astrocapture.sqlite3`, na pasta de dados do usuário indicada por `QStandardPaths::AppDataLocation`. A versão do esquema será registrada em `PRAGMA user_version`; cada migração será executada em uma transação.

Enums serão persistidos como texto e validados por restrições `CHECK`. Timestamps serão armazenados em ISO 8601 UTC. Chaves estrangeiras restringirão exclusões de registros ainda referenciados.

O estado operacional aberto/fechado da sessão será armazenado em coluna própria, separado da etapa de trabalho. Uma nova sessão começa aberta; seu estado permanece entre reinicializações até fechamento explícito. A criação do alvo e da sessão ocorre na mesma transação. Repositórios de domínio definem os contratos, e adaptadores SQLite implementam as consultas e atualizações; transições de sequência são validadas pelas regras do domínio. Timestamps usam seis dígitos de precisão fracionária (microssegundos).

As imagens serão armazenadas em `<AppDataLocation>/images/targets/<target-id>_<target-name-slug>/sessions/<session-id>/nights/<YYYY-MM-DD>/<FrameType>/`. IDs serão codificados em percent-encoding UTF-8 nos caminhos e nomes de arquivos utilizarão o ID do quadro com a extensão original. Se o nome existir, serão tentados sufixos `_001`, `_002` e seguintes antes da extensão. A geração de caminho não reserva o nome; a escrita deverá continuar protegida contra sobrescritas concorrentes.

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