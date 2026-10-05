#include "storage/Storage.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "config/Config.h"

namespace {
bool s_ok = false;
volatile bool s_uploading = false;

const char *cardTypeName(uint8_t type) {
  switch (type) {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SD";
    case CARD_SDHC: return "SDHC";
    default: return "?";
  }
}
}  // namespace

bool storage::begin() {
  namespace pin = cfg::pin;
  namespace path = cfg::path;
  SPI.begin(pin::SD_SCK, pin::SD_MISO, pin::SD_MOSI, pin::SD_CS);

  // SD.begin() pode retornar true com o cartao respondendo mal, entao so vale
  // se o tipo do cartao for reconhecido.
  s_ok = false;
  for (uint32_t freq : cfg::sd::SD_FREQS_HZ) {
    if (SD.begin(pin::SD_CS, SPI, freq, "/sd", cfg::sd::SD_MAX_FILES) &&
        SD.cardType() != CARD_NONE) {
      s_ok = true;
      Serial.printf("[SD] barramento a %lu MHz\n", (unsigned long)(freq / 1000000));
      break;
    }
    SD.end();
  }

  if (!s_ok) {
    Serial.println("[SD] FALHOU ao montar. Checar:");
    Serial.printf("  1. Fiacao: CS=%d SCK=%d MISO=%d MOSI=%d\n",
                  pin::SD_CS, pin::SD_SCK, pin::SD_MISO, pin::SD_MOSI);
    Serial.println("  2. Alimentacao do modulo (a maioria quer 5V no VCC)");
    Serial.println("  3. Cartao formatado em FAT32 (nao exFAT/NTFS)");
    Serial.println("  4. Cartao bem encaixado no soquete");
    return false;
  }

  SD.mkdir(path::DATA_DIR);
  Serial.printf("[SD] ok - tipo %s, %llu MB\n", cardTypeName(SD.cardType()),
                SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

bool storage::ok() { return s_ok; }

void storage::setUploading(bool active) { s_uploading = active; }
bool storage::uploading() { return s_uploading; }
