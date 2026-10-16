#!/usr/bin/env python3
"""Compara uma playlist do Spotify com as musicas do cartao do Localfy e baixa as faltantes.

Lista as faixas da playlist que ainda NAO estao no cartao, grava a lista em
faltando.csv (artista, titulo, album, link do Spotify) e baixa as faixas
faltantes via yt-dlp diretamente para o cartão.

A playlist e lida pela API oficial do Spotify. Crie um app (gratuito) em
https://developer.spotify.com/dashboard e exporte as credenciais:

    export SPOTIFY_CLIENT_ID=...
    export SPOTIFY_CLIENT_SECRET=...

Uso:
    python3 tools/playlist_faltando.py <link-ou-id-da-playlist> /media/<usuario>/<cartao>

O cartao pode ser a raiz (com /music) ou a propria pasta de musicas. A
comparacao usa titulo + artista das etiquetas ID3 (ffprobe) e, sem etiqueta,
o nome do arquivo. Precisa do ffprobe (pacote ffmpeg) e yt-dlp.
"""
import base64
import csv
import difflib
import json
import os
import re
import subprocess
import sys
import unicodedata
import urllib.parse
import urllib.request

API = "https://api.spotify.com/v1"


# --------------------------------------------------------------------------
# Spotify
# --------------------------------------------------------------------------
def token():
    cid = os.environ.get("SPOTIFY_CLIENT_ID")
    secret = os.environ.get("SPOTIFY_CLIENT_SECRET")
    if not cid or not secret:
        sys.exit("Defina SPOTIFY_CLIENT_ID e SPOTIFY_CLIENT_SECRET (veja o topo do arquivo).")
    auth = base64.b64encode(f"{cid}:{secret}".encode()).decode()
    req = urllib.request.Request(
        "https://accounts.spotify.com/api/token",
        data=b"grant_type=client_credentials",
        headers={"Authorization": f"Basic {auth}",
                 "Content-Type": "application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req) as r:
        return json.load(r)["access_token"]


def playlist_id(link):
    m = re.search(r"playlist[/:]([A-Za-z0-9]+)", link)
    return m.group(1) if m else link.strip()


def playlist_tracks(pid, tok):
    fields = "next,items(track(name,artists(name),album(name),external_urls(spotify)))"
    url = f"{API}/playlists/{pid}/tracks?limit=100&fields={urllib.parse.quote(fields)}"
    tracks = []
    while url:
        req = urllib.request.Request(url, headers={"Authorization": f"Bearer {tok}"})
        try:
            with urllib.request.urlopen(req) as r:
                page = json.load(r)
        except urllib.error.HTTPError as e:
            if e.code == 404:
                sys.exit("Playlist nao encontrada. Ela precisa ser publica (ou sua).")
            raise
        for item in page.get("items", []):
            t = item.get("track")
            if not t or not t.get("name"):
                continue   # faixa local ou removida
            tracks.append({
                "title": t["name"],
                "artists": [a["name"] for a in t.get("artists", [])],
                "album": (t.get("album") or {}).get("name", ""),
                "url": (t.get("external_urls") or {}).get("spotify", ""),
            })
        url = page.get("next")
    return tracks


# --------------------------------------------------------------------------
# Download (yt-dlp)
# --------------------------------------------------------------------------
def sanitize_filename(name):
    """Remove caracteres invalidos para sistemas de arquivos como FAT32/exFAT."""
    return re.sub(r'[\\/*?:"<>|]', "", name).strip()


def download_track(track, dest_dir):
    """Baixa o audio em MP3 usando yt-dlp a partir da busca do artista e titulo."""
    artist = ", ".join(track["artists"])
    title = track["title"]
    filename = sanitize_filename(f"{artist} - {title}.mp3")
    out_path = os.path.join(dest_dir, filename)

    if os.path.exists(out_path):
        print(f"  [=] Arquivo ja existe: {filename}")
        return True

    print(f"  [>] Baixando: {artist} - {title}...")

    search_query = f"{artist} - {title}"
    cmd = [
        "yt-dlp",
        "-x",
        "--audio-format", "mp3",
        "--audio-quality", "0",
        "-o", out_path,
        "--no-playlist",
        "--quiet",
        f"ytsearch1:{search_query}"
    ]

    try:
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode == 0 and os.path.exists(out_path):
            print(f"  [OK] Baixado: {filename}")
            return True
        else:
            print(f"  [X] Erro ao baixar '{title}': {res.stderr.strip() or 'Falha no download'}")
            return False
    except FileNotFoundError:
        sys.exit("\nErro: 'yt-dlp' nao esta instalado. Instale executando: pip install yt-dlp")


# --------------------------------------------------------------------------
# Comparacao
# --------------------------------------------------------------------------
VERSION_WORDS = (r"remaster(ed)?|version|single|radio edit|edit|mix|live|mono|stereo|"
                 r"acoustic|demo|bonus|deluxe|explicit|clean|from|original|soundtrack|ost|"
                 r"as featured|feat|ft|with|prod")

TITLE_MATCH = 0.88      # semelhanca minima entre titulos (0 a 1)
TITLE_ALONE = 0.95      # sem artista para conferir, exige titulo quase identico


def norm(text):
    """Forma comparavel: sem acento, minusculas, sem versoes e participacoes."""
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode().lower()
    text = text.replace("&", " and ").replace("_", " ")
    text = re.sub(r"[\(\[][^)\]]*(%s)[^)\]]*[\)\]]" % VERSION_WORDS, " ", text)
    text = re.sub(r"\s[-\u2013]\s[^-]*(%s|\b(19|20)\d\d\b).*$" % VERSION_WORDS, " ", text)
    text = re.sub(r"[^a-z0-9]+", " ", text)
    text = re.sub(r"^the ", "", text.strip())
    return " ".join(text.split())


def similar(a, b):
    """Semelhanca 0..1: o maior entre a sequencia de letras e as palavras em comum."""
    if not a or not b:
        return 0.0
    if a == b:
        return 1.0
    seq = difflib.SequenceMatcher(None, a, b).ratio()
    wa, wb = set(a.split()), set(b.split())
    words = len(wa & wb) / max(len(wa), len(wb))
    return max(seq, words)


def find_cards():
    """Cartoes do Localfy montados no computador (tem a pasta music/)."""
    base = os.path.join("/media", os.environ.get("USER", ""))
    if not os.path.isdir(base):
        return []
    return [os.path.join(base, d) for d in sorted(os.listdir(base))
            if os.path.isdir(os.path.join(base, d, "music"))]


def find_music_dir(path):
    """Pasta de musicas a partir do caminho dado (raiz do cartao ou a propria pasta)."""
    if not os.path.isdir(path):
        cards = find_cards()
        hint = ("\nCartao encontrado: " + ", ".join(cards)) if cards else \
               "\nNenhum cartao montado em /media. Coloque o cartao no computador."
        sys.exit(f"O caminho '{path}' nao existe.{hint}")
    music = os.path.join(path, "music")
    return music if os.path.isdir(music) else path


def card_songs(path):
    """Cada arquivo vira uma lista de (titulo, artista) possiveis, ja normalizados."""
    music = find_music_dir(path)
    songs = []
    for root, _, files in os.walk(music):
        folder = "" if os.path.samefile(root, music) else os.path.basename(root)
        for f in files:
            if not f.lower().endswith((".mp3", ".wav")) or f.startswith("."):
                continue
            full = os.path.join(root, f)
            tags = {}
            if f.lower().endswith(".mp3"):
                out = subprocess.run(
                    ["ffprobe", "-v", "error", "-show_entries", "format_tags=title,artist",
                     "-of", "json", full], capture_output=True, text=True).stdout
                tags = {k.lower(): v for k, v in
                        json.loads(out or "{}").get("format", {}).get("tags", {}).items()}
            stem = re.sub(r"\.[a-z0-9]+$", "", f, flags=re.I)
            stem = re.sub(r"_[a-z0-9-]+\.(org|com|net|io|cc|to)$", "", stem, flags=re.I)

            artists = {norm(x) for x in (tags.get("artist", ""), folder) if x}
            titles = {norm(stem)}
            if tags.get("title"):
                titles.add(norm(tags["title"]))
            if " - " in stem:
                left, right = stem.split(" - ", 1)
                titles.add(norm(right))
                artists.add(norm(left))
            songs.append({"file": os.path.relpath(full, music),
                          "titles": {t for t in titles if t},
                          "artists": {a for a in artists if a}})
    return songs


def artist_matches(track_artists, card_artists):
    for a in track_artists:
        for c in card_artists:
            if a and c and (a in c or c in a or similar(a, c) >= 0.85):
                return True
    return False


def best_match(track, songs):
    """Melhor arquivo do cartao para a faixa: (pontuacao, arquivo) ou (0, None)."""
    title = norm(track["title"])
    artists = [norm(a) for a in track["artists"]]
    best = (0.0, None)
    for s in songs:
        score = max((similar(title, t) for t in s["titles"]), default=0.0)
        if s["artists"]:
            if not artist_matches(artists, s["artists"]):
                continue
            if score < TITLE_MATCH:
                continue
        elif score < TITLE_ALONE:
            continue
        if score > best[0]:
            best = (score, s["file"])
    return best


def on_card(track, songs):
    return best_match(track, songs)[1] is not None


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)

    card_path = sys.argv[2]
    songs = card_songs(card_path)
    if not songs:
        sys.exit(f"Nenhuma musica encontrada em '{find_music_dir(card_path)}'. "
                 "Confira o caminho do cartao (ex.: /media/emo/1EDC-F930).")
    print(f"Cartao: {len(songs)} musicas lidas em {find_music_dir(card_path)}")
    tracks = playlist_tracks(playlist_id(sys.argv[1]), token())
    missing, probable = [], []
    for t in tracks:
        score, file = best_match(t, songs)
        if file is None:
            missing.append(t)
        elif score < 1.0:
            probable.append((score, t, file))

    if probable:
        print("Encontradas por semelhanca (confira se estao certas):")
        for score, t, file in sorted(probable, key=lambda x: x[0]):
            print(f"  {score:.0%}  {', '.join(t['artists'])} - {t['title']}\n        = {file}")
        print()

    print(f"Playlist: {len(tracks)} faixas | no cartao: {len(tracks) - len(missing)} "
          f"| faltando: {len(missing)}\n")

    for t in missing:
        print(f"  {', '.join(t['artists'])} - {t['title']}")

    with open("faltando.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["artista", "titulo", "album", "spotify"])
        for t in missing:
            w.writerow([", ".join(t["artists"]), t["title"], t["album"], t["url"]])
    print("\nLista gravada em faltando.csv")

    if missing:
        music_dir = find_music_dir(card_path)

        resp = input(f"\nDeseja baixar as {len(missing)} faixas faltantes para '{music_dir}'? [s/N]: ")
        if resp.lower().startswith("s"):
            print("\n--- Iniciando Downloads via yt-dlp ---")
            success_count = 0
            for t in missing:
                if download_track(t, music_dir):
                    success_count += 1
            print(f"\nConcluido! {success_count}/{len(missing)} faixas baixadas.")


if __name__ == "__main__":
    main()