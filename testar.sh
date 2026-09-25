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

for b in xdg-open brave-browser firefox google-chrome chromium; do
  if command -v "$b" >/dev/null 2>&1; then
    "$b" "$URL" >/dev/null 2>&1 &
    echo "Abrindo com $b..."
    exit 0
  fi
done
echo "Nenhum navegador encontrado - copie a URL acima."
