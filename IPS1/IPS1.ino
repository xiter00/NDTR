#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <LittleFS.h>   
#include <TJpg_Decoder.h>

// --- KONFIGURASI PIN ESP32-S3 KE IPS 1.14 ---
#define TFT_CS    10  
#define TFT_DC    9  
#define TFT_RST   8

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Callback untuk TJpgDec
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

// ========================================================
// FUNGSI DEWA: EFEK CYBERPUNK GLITCH TEKS SEGEDE GABAN
// ========================================================
void tampilkanGlitchAngka(int angka) {
  unsigned long waktuMulai = millis();
  char stringAngka[2];
  sprintf(stringAngka, "%d", angka);

  // Jalankan animasi glitch selama 1 detik (1000ms) per angka
  while (millis() - waktuMulai < 1000) {
    
    // 1. Gambar garis statis/glitch horizontal secara acak di layar
    int yGaris = random(0, 240);
    int hGaris = random(2, 10); // ketebalan garis acak
    uint16_t warnaGaris = (random(0, 2) == 0) ? ST77XX_CYAN : ST77XX_MAGENTA;
    tft.fillRect(0, yGaris, 135, hGaris, warnaGaris);

    // 2. Efek Chromatic Aberration (Teks bayangan bergeser acak)
    int offsetX = random(-5, 6);
    int offsetY = random(-3, 4);

    tft.setTextSize(9); // UKURAN SEGEDE GABAN! (Skala 9)

    // Cetak Bayangan Cyan (Geser Kiri/Atas)
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(45 + offsetX, 85 + offsetY);
    tft.print(stringAngka);

    // Cetak Bayangan Magenta (Geser Kanan/Bawah)
    tft.setTextColor(ST77XX_MAGENTA);
    tft.setCursor(45 - offsetX, 85 - offsetY);
    tft.print(stringAngka);

    // Cetak Teks Utama Warna Putih di Tengah
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(45, 85);
    tft.print(stringAngka);

    delay(40); // Kecepatan kedipan glitch (makin kecil makin cepet/liar)

    // 3. Clear layar acak biar efek kedip rusaknya dapet
    if (random(0, 10) > 6) {
      tft.fillScreen(ST77XX_BLACK);
    }
  }
  // Clear total sebelum masuk ke angka berikutnya
  tft.fillScreen(ST77XX_BLACK);
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi Layar IPS ST7789 (135x240)
  tft.init(135, 240);
  tft.setRotation(0); // Tegak (Portrait)
  tft.fillScreen(ST77XX_BLACK);

  // --------------------------------------------------------
  // PROSES COOLDOWN 3 DETIK DENGAN EFEK GLITCH DI LAYAR IPS
  // --------------------------------------------------------
  tampilkanGlitchAngka(3); // Glitch angka 3 selama 1 detik
  tampilkanGlitchAngka(2); // Glitch angka 2 selama 1 detik
  tampilkanGlitchAngka(1); // Glitch angka 1 selama 1 detik

  // Efek flash putih pas mau mulai (Biar kayak ledakan TV jadul)
  tft.fillScreen(ST77XX_WHITE);
  delay(100);
  tft.fillScreen(ST77XX_BLACK);

  // Inisialisasi LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Gagal memuat LittleFS!");
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 100);
    tft.print("FS ERROR!");
    while (1) delay(1000);
  }

  // Konfigurasi Decoder JPEG
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
}

void loop() {
  File videoFile = LittleFS.open("/video.mjpeg", "r");
  
  if (!videoFile) {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 100);
    tft.print("NO VIDEO!");
    delay(5000);
    return;
  }

  while (videoFile.available()) {
    if (videoFile.read() == 0xFF && videoFile.peek() == 0xD8) {
      TJpgDec.drawJpg(0, 0, "/video.mjpeg"); 
      delay(40); // Atur FPS video lu di sini
    }
  }

  videoFile.close();
  tft.fillScreen(ST77XX_BLACK);
  delay(500); 
}
