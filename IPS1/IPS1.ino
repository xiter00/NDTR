#include <TFT_eSPI.h>
#include <SPI.h>
#include <LittleFS.h>   
#include <TJpg_Decoder.h>
#include "video_data.h" // Otomatis dibikin sama GitHub Actions

// TFT_eSPI instance (Pin CS, DC, RST disetting langsung dari GitHub Actions)
TFT_eSPI tft = TFT_eSPI();

// Callback untuk TJpgDec
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  
  // pushImage adalah kunci utama 30FPS di TFT_eSPI, jauh lebih cepat dari drawRGBBitmap
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

// ========================================================
// FUNGSI DEWA: EFEK CYBERPUNK GLITCH (VERSI LANDSCAPE/TIDUR)
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

    tft.setTextSize(9); // Tinggi font ~72px

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

  // Inisialisasi Layar TFT_eSPI
  tft.init();
  tft.setRotation(1); // Tegak (Portrait ditarik jadi Landscape)
  tft.fillScreen(TFT_BLACK);

  // --------------------------------------------------------
  // PROSES COOLDOWN 3 DETIK DENGAN EFEK GLITCH DI LAYAR IPS
  // --------------------------------------------------------
  tampilkanGlitchAngka(3); 
  tampilkanGlitchAngka(2); 
  tampilkanGlitchAngka(1); 

  tft.fillScreen(TFT_WHITE);
  delay(100);
  tft.fillScreen(TFT_BLACK);

  // Konfigurasi Decoder JPEG
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); // WAJIB DI TFT_eSPI: Biar warnanya gak biru jadi merah (terbalik)
  TJpgDec.setCallback(tft_output);
}

void loop() {
  uint32_t index = 0;
  
  // Mesin pencari frame video langsung dari Hex Array
  while (index < video_size - 1) {
    if (video_mjpeg[index] == 0xFF && video_mjpeg[index + 1] == 0xD8) {
      uint32_t start_mcu = index;
      index += 2;
      
      while (index < video_size - 1) {
        if (video_mjpeg[index] == 0xFF && video_mjpeg[index + 1] == 0xD9) {
          uint32_t end_mcu = index + 1;
          uint32_t frame_size = end_mcu - start_mcu + 1;

          // Render gambar ke layar
          TJpgDec.drawJpg(0, 0, video_mjpeg + start_mcu, frame_size);
          
          // Lompati pembacaan index agar tak mengulang iterasi yang tak perlu
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
