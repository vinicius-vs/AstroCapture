# ADR-006: Preservação dos arquivos RAW originais

- **Status:** Aceito
- **Data:** 2026-10-17

## Contexto

As imagens capturadas são os dados originais da sessão astronômica. O empilhamento, a calibração e a edição poderão gerar novos arquivos, mas não deverão comprometer os dados de origem.

## Decisão

Preservar os arquivos originais transferidos da câmera, sem conversão, recompressão ou alteração de seus bytes durante a captura.

O arquivo original deverá manter sua extensão e ser armazenado separadamente de miniaturas, pré-visualizações e resultados de processamento.

## Justificativa

A preservação permite repetir processamentos futuros com algoritmos diferentes e comparar resultados sem perder informação da captura original.

## Consequências

- O AstroCapture não deverá modificar o arquivo RAW original para gerar uma pré-visualização.
- Miniaturas e imagens processadas deverão ser armazenadas separadamente.
- A validação deverá detectar arquivos ausentes, vazios ou incompletos.
- Poderá ser calculado um hash para verificar a integridade dos arquivos.
- Falhas de transferência não deverão resultar em quadros marcados como concluídos.
- Os módulos de empilhamento e edição deverão tratar os originais como fontes de leitura, produzindo novos arquivos para os resultados.