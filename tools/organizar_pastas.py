#!/usr/bin/env python3
"""Padroniza e organiza as musicas do cartao do Localfy por artista.

Cada musica vai para /music/<Artista>/<Titulo>.<ext>:
  - o artista vem das etiquetas ID3 (artista do album, senao artista) ou, sem
    etiqueta, do nome "Artista - Titulo.mp3". So o artista principal: o que
    vem depois de virgula ou "feat." e participacao. "&" NAO separa ("Daryl
    Hall & John Oates" e um artista so);
  - o titulo vem da etiqueta ou do nome do arquivo, sem o sufixo de site
    ("_spotdown.org"), sem o artista repetido no inicio e sem "- 2004
    Remaster". Versoes de verdade ("Dave Bascombe 7" Mix", "feat.") ficam;
  - caracteres que o cartao (FAT) nao aceita sao trocados.

Pastas que ja existem sao reaproveitadas sem diferenciar maiusculas. O
/localfy/meta.json (artista e genero editados na interface) e o
/localfy/history.csv (historico de escutas) acompanham os nomes novos.
Arquivos que nao sao audio valido ficam onde estao e sao listados.

Uso:  python3 tools/organizar_pastas.py /media/<usuario>/<cartao>            (so mostra o plano)
      python3 tools/organizar_pastas.py /media/<usuario>/<cartao> --aplicar
Precisa do ffprobe (pacote ffmpeg).
"""
import csv
import io
import json
import os
import re
import subprocess
import sys

AUDIO = (".mp3", ".wav")
MAX_FOLDER = 48     # limites do firmware (Library.cpp)
MAX_NAME = 112


def probe(path):
    """(etiquetas em minusculas, duracao em s) ou ({}, 0) se nao for audio."""
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries",
         "format=duration:format_tags=title,artist,album_artist", "-of", "json", path],
        capture_output=True, text=True).stdout
    fmt = json.loads(out or "{}").get("format", {})
    tags = {k.lower(): v.strip() for k, v in fmt.get("tags", {}).items()}
    try:
        dur = float(fmt.get("duration", 0))
    except ValueError:
        dur = 0
    return tags, dur


def main_artist(names):
    """Artista principal de "A, B" / "A feat. B" / "A; B" (sem quebrar em "&")."""
    return re.split(r"\s*(?:,|;| feat\.? | ft\.? )\s*", names, maxsplit=1)[0].strip()


def fat_safe(text, limit):
    """Nome valido no FAT e no firmware: sem / \\ : * ? " < > |, sem ponto inicial."""
    text = text.replace("/", " - ").replace("\\", "-").replace(":", " -").replace('"', "'")
    text = re.sub(r'[*?<>|]', "", text)
    text = text.replace("..", ".")
    text = re.sub(r"\s+", " ", text).strip().lstrip(".").strip()
    text = re.sub(r"(\s-)+$", "", text).rstrip(". ")
    return text[:limit].rstrip(". ")


def clean_title(title, artist):
    t = re.sub(r"_[A-Za-z0-9-]+\.(org|com|net|io|cc|to)$", "", title)       # _spotdown.org
    if artist and t.lower().startswith(artist.lower() + " - "):              # "Joji - X" -> "X"
        t = t[len(artist) + 3:]
    t = re.sub(r"\s-\s((19|20)\d\d\s)?Remaster(ed)?(\s(19|20)\d\d)?$", "", t, flags=re.I)
    t = re.sub(r"\s\((Remaster(ed)?\s)?(19|20)\d\d\sRemaster(ed)?\)$", "", t, flags=re.I)
    return t.strip()


def plan_file(music, rel, existing_folders):
    full = os.path.join(music, rel)
    base, ext = os.path.splitext(os.path.basename(rel))
    folder_now = os.path.dirname(rel)
    tags, dur = probe(full)
    if dur <= 0:
        return None   # nao e audio valido

    stem = re.sub(r"_[A-Za-z0-9-]+\.(org|com|net|io|cc|to)$", "", base)
    artist = main_artist(tags.get("album_artist") or tags.get("artist") or "")
    title = tags.get("title", "")
    # "Artistas - Titulo.mp3" so vale solto na raiz: dentro da pasta do
    # artista o nome ja e o titulo ("Harness Your Hopes - B-side").
    if not artist and not folder_now and " - " in stem:
        names, title_from_name = stem.split(" - ", 1)
        artist = main_artist(names)
        title = title or title_from_name
    artist = artist or folder_now
    title = clean_title(title or stem, artist)

    if not artist:
        folder = ""                                # continua solta ("Sem pasta")
    else:
        safe = fat_safe(artist, MAX_FOLDER)
        folder = existing_folders.setdefault(safe.lower(), safe)
    room = MAX_NAME - (len(folder) + 1 if folder else 0) - len(ext)
    name = fat_safe(title, room) + ext.lower()
    return os.path.join(folder, name) if folder else name


def rename_in_meta(path, mapping):
    if not os.path.exists(path):
        return
    with open(path, encoding="utf-8") as fh:
        meta = json.load(fh)
    meta = {mapping.get(k, k): v for k, v in meta.items()}
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(meta, fh, ensure_ascii=False)
    os.replace(tmp, path)


def rename_in_history(path, mapping):
    if not os.path.exists(path):
        return
    with open(path, encoding="utf-8", newline="") as fh:
        rows = list(csv.reader(fh))
    if not rows or "file" not in rows[0]:
        return
    col = rows[0].index("file")
    for r in rows[1:]:
        if len(r) > col:
            r[col] = mapping.get(r[col], r[col])
    out = io.StringIO()
    csv.writer(out, quoting=csv.QUOTE_MINIMAL, lineterminator="\n").writerows(rows)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="") as fh:
        fh.write(out.getvalue())
    os.replace(tmp, path)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    raiz = sys.argv[1]
    aplicar = "--aplicar" in sys.argv
    music = os.path.join(raiz, "music")
    if not os.path.isdir(music):
        sys.exit(f"nao achei {music}: e o cartao do Localfy?")

    existing = {d.lower(): d for d in os.listdir(music)
                if os.path.isdir(os.path.join(music, d)) and not d.startswith(".")}
    files = []
    for root, _, names in os.walk(music):
        for n in sorted(names):
            if n.lower().endswith(AUDIO) and not n.startswith("."):
                files.append(os.path.relpath(os.path.join(root, n), music))

    mapping, invalid, taken = {}, [], set()
    for rel in sorted(files):
        target = plan_file(music, rel, existing)
        if target is None:
            invalid.append(rel)
            continue
        base, ext = os.path.splitext(target)
        n = 2
        while target.lower() in taken:             # dois arquivos com o mesmo titulo
            target = f"{base} ({n}){ext}"
            n += 1
        taken.add(target.lower())
        mapping[rel] = target

    changes = {a: b for a, b in mapping.items() if a != b}
    by_folder = {}
    for a, b in sorted(mapping.items(), key=lambda kv: kv[1].lower()):
        by_folder.setdefault(os.path.dirname(b) or "(sem pasta)", []).append((a, b))
    for folder, items in by_folder.items():
        print(f"{folder}/ ({len(items)})")
        for a, b in items:
            mark = "  " if a == b else "->"
            print(f"   {mark} {os.path.basename(b)}" + ("" if a == b else f"    [de: {a}]"))
    print(f"\n{len(mapping)} musicas em {len(by_folder)} pastas; {len(changes)} serao renomeadas/movidas.")
    if invalid:
        print(f"\n{len(invalid)} arquivos NAO sao audio valido (ficam como estao):")
        for f in invalid:
            print(f"   {f}  ({os.path.getsize(os.path.join(music, f))} bytes)")

    if not aplicar:
        print("\nNada foi alterado. Rode de novo com --aplicar.")
        return

    # Duas etapas (nome temporario) para trocas que so mudam maiusculas ou se cruzam.
    temps = {}
    for i, (a, b) in enumerate(changes.items()):
        tmp = os.path.join(os.path.dirname(a), f".localfy-tmp-{i}{os.path.splitext(a)[1]}")
        os.rename(os.path.join(music, a), os.path.join(music, tmp))
        temps[tmp] = b
    for tmp, b in temps.items():
        os.makedirs(os.path.join(music, os.path.dirname(b)), exist_ok=True)
        os.rename(os.path.join(music, tmp), os.path.join(music, b))
    for d in os.listdir(music):                       # pastas que ficaram vazias
        p = os.path.join(music, d)
        if os.path.isdir(p) and not os.listdir(p):
            os.rmdir(p)

    rename_in_meta(os.path.join(raiz, "localfy", "meta.json"), changes)
    rename_in_history(os.path.join(raiz, "localfy", "history.csv"), changes)
    os.sync()
    print("Pronto. Ejete o cartao com seguranca antes de tirar.")


if __name__ == "__main__":
    main()
