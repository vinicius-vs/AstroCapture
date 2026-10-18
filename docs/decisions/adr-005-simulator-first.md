# ADR-005: Desenvolvimento com câmera simulada primeiro

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

O desenvolvimento e os testes não devem depender continuamente da disponibilidade de uma câmera física, de cabos, de permissões USB ou do suporte específico do hardware.

## Decisão

Implementar primeiro um simulador de câmera, antes de concluir a integração física com o libgphoto2.

O simulador terá dois modos:

### Modo básico

Permitirá testar conexão simulada, configurações, capturas individuais, sequências e persistência de imagens de teste.

### Modo realista

Permitirá configurar tempos de exposição e transferência, além de reproduzir falhas de exposição, transferência, desconexão e arquivos inválidos.

Os cenários de falha deverão ser reproduzíveis para permitir testes automatizados determinísticos.

## Justificativa

O simulador permite validar a arquitetura, a interface, o armazenamento e a recuperação de falhas sem depender do hardware.

## Consequências

- As regras de captura deverão funcionar igualmente com o simulador e com a câmera física.
- Os testes unitários e de integração deverão utilizar o simulador sempre que possível.
- Os erros simulados deverão seguir os mesmos contratos de erro utilizados pelo adaptador físico.
- A integração real será validada separadamente, considerando as capacidades efetivamente suportadas pela câmera.