# ADR-001: C++ e Qt Widgets

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

O AstroCapture é uma aplicação desktop para astrofotografia. O software precisa controlar câmeras, executar sequências de captura, apresentar imagens e, futuramente, realizar empilhamento e edição.

A aplicação deve oferecer uma interface responsiva, acesso a bibliotecas nativas e uma arquitetura adequada à evolução do projeto.

## Decisão

Utilizar C++ como linguagem principal e Qt 6 Widgets para a interface gráfica.

O projeto utilizará CMake para configuração e compilação.

## Justificativa

- C++ oferece controle sobre recursos e integração com bibliotecas nativas.
- Qt fornece componentes para interfaces desktop, gerenciamento de eventos e concorrência.
- CMake permite organizar dependências e builds de maneira reproduzível.
- A combinação atende aos objetivos de aprendizado e desenvolvimento de um software desktop real.

## Consequências

- A interface deve permanecer desacoplada da lógica de captura.
- Operações demoradas não devem bloquear a thread principal.
- A configuração do ambiente de desenvolvimento deverá documentar as dependências necessárias.
- O código deverá ser organizado em módulos para facilitar manutenção e testes.