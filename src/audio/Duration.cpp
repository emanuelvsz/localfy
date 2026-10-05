#include "audio/Duration.h"

#include <string.h>

uint32_t duration::ofWav(File &f) {
  uint8_t hdr[44];
  size_t pos = f.position();
  f.seek(0);
  if (f.read(hdr, 44) != 44) {
    f.seek(pos);
    return 0;
  }
  f.seek(pos);
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return 0;
  uint32_t byteRate = hdr[28] | (hdr[29] << 8) | (hdr[30] << 16) | (hdr[31] << 24);
  if (byteRate == 0) return 0;
  return (f.size() - 44) / byteRate;
}

uint32_t duration::ofMp3(File &f) {
  static const int kBitratesKbps[16] = {0,   32,  40,  48,  56,  64,  80,  96,
                                        112, 128, 160, 192, 224, 256, 320, 0};
  size_t pos = f.position();
  f.seek(0);
  uint8_t b[4];
  uint32_t scanned = 0;
  // Procura o primeiro cabecalho de quadro (11 bits em 1) nos primeiros 8 KB.
  while (f.available() && scanned < 8192) {
    if (f.read(b, 1) != 1) break;
    scanned++;
    if (b[0] != 0xFF) continue;
    if (f.read(b + 1, 3) != 3) break;
    scanned += 3;
    if ((b[1] & 0xE0) != 0xE0) {
      f.seek(f.position() - 3);
      scanned -= 3;
      continue;
    }
    int kbps = kBitratesKbps[(b[2] >> 4) & 0x0F];
    if (kbps > 0) {
      f.seek(pos);
      return (f.size() * 8) / (kbps * 1000);
    }
  }
  f.seek(pos);
  return 0;
}
