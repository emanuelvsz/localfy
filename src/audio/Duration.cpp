#include "audio/Duration.h"

#include <string.h>

namespace {
uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Cabecalho de quadro MPEG Audio Layer III.
struct Frame {
  bool ok = false;
  int version = 0;        // 3 = MPEG1, 2 = MPEG2, 0 = MPEG2.5
  uint32_t kbps = 0;
  uint32_t sampleRate = 0;
  uint32_t length = 0;    // bytes do quadro inteiro
  uint32_t samples = 0;   // amostras por quadro
  bool mono = false;
};

Frame parseHeader(const uint8_t *h) {
  static const uint16_t kRate1[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
  static const uint16_t kRate2[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
  static const uint32_t kSr[3][3] = {{11025, 12000, 8000},     // MPEG2.5
                                     {22050, 24000, 16000},    // MPEG2
                                     {44100, 48000, 32000}};   // MPEG1
  Frame f;
  if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0) return f;
  int version = (h[1] >> 3) & 3;
  int layer = (h[1] >> 1) & 3;
  int rateIdx = h[2] >> 4;
  int srIdx = (h[2] >> 2) & 3;
  if (version == 1 || layer != 1 || rateIdx == 0 || rateIdx == 15 || srIdx == 3) return f;

  f.version = version;
  f.kbps = version == 3 ? kRate1[rateIdx] : kRate2[rateIdx];
  f.sampleRate = kSr[version == 3 ? 2 : version == 2 ? 1 : 0][srIdx];
  int padding = (h[2] >> 1) & 1;
  f.samples = version == 3 ? 1152 : 576;
  f.length = (version == 3 ? 144 : 72) * f.kbps * 1000 / f.sampleRate + padding;
  f.mono = (h[3] >> 6) == 3;
  f.ok = f.length >= 24;
  return f;
}

// Tamanho da etiqueta ID3v2 no inicio do arquivo (0 se nao houver).
uint32_t id3Size(File &file) {
  uint8_t h[10];
  file.seek(0);
  if (file.read(h, 10) != 10 || memcmp(h, "ID3", 3) != 0) return 0;
  // Tamanho "syncsafe": 7 bits por byte.
  uint32_t size = ((uint32_t)(h[6] & 0x7F) << 21) | ((uint32_t)(h[7] & 0x7F) << 14) |
                  ((uint32_t)(h[8] & 0x7F) << 7) | (h[9] & 0x7F);
  return 10 + size + ((h[5] & 0x10) ? 10 : 0);   // + rodape, se houver
}

// Primeiro quadro valido a partir de 'from', confirmado pelo quadro seguinte
// (um par FF Ex isolado aparece a toda hora em JPEG e em lixo).
bool findFirstFrame(File &file, uint32_t from, uint32_t &at, Frame &frame) {
  const uint32_t kMaxScan = 64 * 1024;
  uint8_t buf[512];
  uint32_t base = from;
  while (base < from + kMaxScan && base < file.size()) {
    file.seek(base);
    int n = file.read(buf, sizeof(buf));
    if (n < 4) return false;
    for (int i = 0; i + 4 <= n; i++) {
      if (buf[i] != 0xFF) continue;
      Frame f = parseHeader(buf + i);
      if (!f.ok) continue;
      uint32_t pos = base + i;
      uint8_t next[4];
      file.seek(pos + f.length);
      if (file.read(next, 4) == 4) {
        Frame g = parseHeader(next);
        if (!g.ok || g.version != f.version || g.sampleRate != f.sampleRate) continue;
      }
      at = pos;
      frame = f;
      return true;
    }
    base += n - 3;   // sobreposicao para nao perder um cabecalho na emenda
  }
  return false;
}

// Duracao exata pelo cabecalho de taxa variavel, se o primeiro quadro tiver um.
uint32_t vbrSeconds(File &file, uint32_t at, const Frame &f) {
  uint8_t b[48];
  file.seek(at);
  if (file.read(b, sizeof(b)) != (int)sizeof(b)) return 0;
  // Xing/Info: depois do cabecalho (4) e das "side info".
  int side = f.version == 3 ? (f.mono ? 17 : 32) : (f.mono ? 9 : 17);
  const uint8_t *x = b + 4 + side;
  if (memcmp(x, "Xing", 4) == 0 || memcmp(x, "Info", 4) == 0) {
    uint32_t flags = be32(x + 4);
    if (flags & 1) {
      uint32_t frames = be32(x + 8);
      return (uint32_t)((uint64_t)frames * f.samples / f.sampleRate);
    }
  }
  // VBRI (Fraunhofer): sempre 32 bytes depois do cabecalho.
  const uint8_t *v = b + 36;
  if (memcmp(v, "VBRI", 4) == 0) {
    uint32_t frames = be32(v + 14);
    return (uint32_t)((uint64_t)frames * f.samples / f.sampleRate);
  }
  return 0;
}
}  // namespace

duration::Info duration::ofWav(File &f) {
  Info info;
  info.audioStart = 44;
  uint8_t hdr[44];
  size_t pos = f.position();
  f.seek(0);
  bool ok = f.read(hdr, 44) == 44;
  f.seek(pos);
  if (!ok || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return info;
  uint32_t byteRate = hdr[28] | (hdr[29] << 8) | (hdr[30] << 16) | ((uint32_t)hdr[31] << 24);
  info.channels = hdr[22] == 1 ? 1 : 2;
  info.sampleRate = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | ((uint32_t)hdr[27] << 24);
  if (byteRate) {
    info.secs = (f.size() - 44) / byteRate;
    info.exact = true;
  }
  return info;
}

duration::Info duration::ofMp3(File &file) {
  Info info;
  uint32_t tag = id3Size(file);
  uint32_t at = 0;
  Frame f;
  if (!findFirstFrame(file, tag, at, f)) {
    file.seek(0);
    info.audioStart = tag < file.size() ? tag : 0;
    return info;
  }
  info.audioStart = at;
  info.sampleRate = f.sampleRate;
  info.channels = f.mono ? 1 : 2;
  info.secs = vbrSeconds(file, at, f);
  info.exact = info.secs > 0;
  if (info.secs == 0 && f.kbps > 0)
    info.secs = (uint32_t)((uint64_t)(file.size() - at) * 8 / (f.kbps * 1000));
  file.seek(0);
  return info;
}
