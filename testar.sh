#!/usr/bin/env bash
# Abre a interface do Localfy em modo de testes, sem precisar do ESP32.
# Os dados sao simulados no proprio navegador: biblioteca, player, bluetooth,
# upload com progresso, scrobble e estatisticas. Nada toca o aparelho.
set -e
cd "$(dirname "$0")"
URL="file://$PWD/index.html?mock=1"

echo "Localfy - modo de testes"
echo "  $URL"
echo
echo "  tecla M       alterna o modo de testes"
echo "  botao amarelo sai do modo de testes"
echo

# Navegador direto, nunca xdg-open: ele trata a URL como nome de arquivo e
# perde o ?mock=1 (e aqui ainda esbarra em libs quebradas do snap).
for b in brave-browser firefox google-chrome chromium chromium-browser; do
  if command -v "$b" >/dev/null 2>&1; then
    echo "Abrindo com $b..."
    nohup "$b" "$URL" >/dev/null 2>&1 &
    sleep 2
    exit 0
  fi
done

echo "Nenhum navegador encontrado."
echo "Copie a URL acima e cole na barra de enderecos."
