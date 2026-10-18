# ADR-002: Abstração do controle de câmera

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

O AstroCapture precisa controlar uma câmera física, inicialmente uma Canon T6, e também permitir desenvolvimento e testes sem equipamento conectado.

Os recursos disponíveis podem variar conforme o modelo, o firmware e o suporte oferecido pelo backend de câmera.

## Decisão

Definir uma interface de domínio chamada `ICamera`, com implementações independentes:

- `GPhotoCamera`: integração com câmeras compatíveis por meio do libgphoto2.
- `SimulatedCamera`: implementação para desenvolvimento e testes.

A interface deverá expor operações como conexão, consulta de capacidades, configuração, captura, transferência de imagens e desconexão, respeitando os recursos realmente disponíveis.

## Justificativa

A abstração reduz o acoplamento entre o fluxo de captura e a biblioteca responsável pelo hardware. Também permite testar regras de negócio sem depender de uma câmera física.

## Consequências

- O código de captura não deverá depender diretamente das APIs do libgphoto2.
- Capacidades não suportadas devem ser identificadas explicitamente.
- Erros de conexão, exposição e transferência devem ser representados de forma consistente.
- O simulador deverá reproduzir o contrato da câmera física, inclusive os erros relevantes para os testes.
- A compatibilidade real deverá ser validada individualmente para cada funcionalidade.