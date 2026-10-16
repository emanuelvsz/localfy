#!/usr/bin/env bash
# Gera as figuras do BMO para o artigo em artigo/figuras/case/.
# Uso: ./gerar_figuras.sh            (todas)
#      ./gerar_figuras.sh frente     (so uma)
set -euo pipefail
cd "$(dirname "$0")"
OUT=../../artigo/figuras/case
mkdir -p "$OUT"

# limites de cada STL (usados para arrumar a figura das pecas em grade)
python3 - <<'PY'
import glob, os
rows = []
for f in sorted(glob.glob('stl/*.stl')):
    xs, ys, zs = [], [], []
    for line in open(f, errors='ignore'):
        if line.lstrip().startswith('vertex'):
            _, x, y, z = line.split(); xs.append(float(x)); ys.append(float(y)); zs.append(float(z))
    n = os.path.basename(f)[:-4]
    rows.append(f'  ["{n}", [{min(xs):.2f}, {min(ys):.2f}, {min(zs):.2f}], [{max(xs):.2f}, {max(ys):.2f}, {max(zs):.2f}]]')
open('stl_limites.scad', 'w').write('// Gerado por gerar_figuras.sh: limites de cada STL [min, max].\nstl_limits = [\n' + ',\n'.join(rows) + '];\n')
PY

# nome | angulos da camera (rx,ry,rz) | projecao (p = perspectiva, o = ortogonal)
FIGS=(
  "geral|70,0,215|p"
  "frente|90,0,180|o"
  "lateral|90,0,90|o"
  "explodida|62,0,35|p"
  "interior|58,0,20|p"
  "tampa|55,0,200|p"
  "base|68,0,215|p"
  "pecas|45,0,15|p"
)

for entry in "${FIGS[@]}"; do
  IFS='|' read -r name rot proj <<< "$entry"
  if [[ $# -gt 0 && "$name" != "$1" ]]; then continue; fi
  echo "-> $name"
  openscad -q -o "$OUT/bmo-$name.png" --imgsize=1600,1200 \
    --camera=0,0,0,$rot,300 --viewall --autocenter --projection=$proj \
    --colorscheme=Tomorrow \
    -D "fig=\"$name\"" -D "cam=[$rot]" figuras.scad
done
