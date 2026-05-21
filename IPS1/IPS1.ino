#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>   
#include <TJpg_Decoder.h>
#include <esp_partition.h> // API native ESP32 untuk baca memory

TFT_eSPI tft = TFT_eSPI();

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// Pointer & ukuran video map
const uint8_t* video_mjpeg = nullptr;
uint32_t video_size = 0;
spi_flash_mmap_handle_t mmap_handle;

// ========================================================
// FUNGSI GLITCH
// ========================================================
void tampilkanGlitchAngka(int angka) {
  unsigned long waktuMulai = millis();
  char stringAngka[2];
  sprintf(stringAngka, "%d", angka);

  while (millis() - waktuMulai < 1000) {
    int yGaris = random(0, 135); 
    int hGaris = random(2, 10);
    uint16_t warnaGaris = (random(0, 2) == 0) ? TFT_CYAN : TFT_MAGENTA;
    tft.fillRect(0, yGaris, 240, hGaris, warnaGaris);
    int offsetX = random(-5, 6);
    int offsetY = random(-3, 4);
    
    tft.setTextSize(9);
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(95 + offsetX, 30 + offsetY);
    tft.print(stringAngka);
    
    tft.setTextColor(TFT_MAGENTA);
    tft.setCursor(95 - offsetX, 30 - offsetY);
    tft.print(stringAngka);
    
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(95, 30);
    tft.print(stringAngka);
    
    delay(40);
    if (random(0, 10) > 6) {
      tft.fillScreen(TFT_BLACK);
    }
  }
  tft.fillScreen(TFT_BLACK);
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  tampilkanGlitchAngka(3); 
  tampilkanGlitchAngka(2); 
  tampilkanGlitchAngka(1); 

  tft.fillScreen(TFT_WHITE);
  delay(100);
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); 
  TJpgDec.setCallback(tft_output);

  // --- MAPPING VIDEO DARI PARTISI FLASH (TANPA BIKIN RAM CRASH) ---
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "video");
  if (part != nullptr) {
    video_size = part->size;
    esp_partition_mmap(part, 0, video_size, SPI_FLASH_MMAP_DATA, (const void**)&video_mjpeg, &mmap_handle);
    Serial.println("SUKSES: Video ditemukan dan di-map ke RAM!");
  } else {
    Serial.println("ERROR: Partisi video tidak ditemukan!");
  }
}

void loop() {
  if (video_mjpeg == nullptr) {
    delay(1000); // Stop kalau video gak ke-load
    return;
  }

  uint32_t index = 0;
  while (index < video_size - 1) {
    if (video_mjpeg[index] == 0xFF && video_mjpeg[index + 1] == 0xD8) {
      uint32_t start_mcu = index;
      index += 2;
      
      while (index < video_size - 1) {
        if (video_mjpeg[index] == 0xFF && video_mjpeg[index + 1] == 0xD9) {
          uint32_t end_mcu = index + 1;
          uint32_t frame_size = end_mcu - start_mcu + 1;

          TJpgDec.drawJpg(0, 0, video_mjpeg + start_mcu, frame_size);
          
          index = end_mcu + 1; 
          break; 
        }
        index++;
      }
    } else {
      index++;
    }
  }
}
