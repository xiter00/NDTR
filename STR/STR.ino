#include <WiFi.h>
#include <WiFiClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>

// --- 1. KONFIGURASI WIFI HOTSPOT HP LU ---
const char* ssid = "NOT MASTAH";     // Ganti pake nama hotspot HP lu!
const char* password = "yangbrorasakan"; // Ganti pake password hotspot lu!

// --- 2. KONFIGURASI IP APK SCREEN STREAM LU ---
const char* host = "192.168.94.60"; 
const int port = 8080;

// --- 3. PIN LAYAR IPS ESP32-S3 LU ---
#define TFT_CS    10  
#define TFT_DC    9  
#define TFT_RST   8
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Buffer buat nyimpen 1 frame gambar (30KB harusnya cukup buat resolusi kecil)
uint8_t frameBuffer[30000]; 
WiFiClient client;

// Callback buat nampilin gambar ke layar
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  Serial.begin(115200);
  
  // Setup Layar
  tft.init(135, 240);
  tft.setRotation(1); // Miring / Landscape
  tft.fillScreen(ST77XX_BLACK);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  // Konek ke Hotspot HP
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("Konek WiFi...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 50);
  tft.print("WIFI KONEK!");
  delay(1000);
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  // Kalau belum konek ke IP APK, paksa konek!
  if (!client.connected()) {
    if (client.connect(host, port)) {
      // Minta jalur stream ke APK (Cek path di APK lu, kadang /stream.mjpeg)
      client.print(String("GET /stream.mjpeg HTTP/1.1\r\n") +
                   "Host: " + host + "\r\n" +
                   "Connection: keep-alive\r\n\r\n");
    } else {
      delay(500);
      return;
    }
  }

  // --- MESIN PENYEDOT FRAME (REVISI DEWA) ---
  // Pake static biar datanya gak ilang pas ESP32 ngambil napas
  static uint32_t bufIdx = 0;
  static bool isFrame = false;
  static uint8_t prevByte = 0;

  // Sedot data selama ada yang dikirim dari HP
  while (client.available()) {
    uint8_t c = client.read();
    
    if (!isFrame) {
      // Nyari kode Awal Gambar JPEG (0xFF 0xD8)
      if (prevByte == 0xFF && c == 0xD8) {
        isFrame = true;
        bufIdx = 0;
        frameBuffer[bufIdx++] = 0xFF;
        frameBuffer[bufIdx++] = 0xD8;
      }
    } else {
      // Simpen ke memori
      frameBuffer[bufIdx++] = c;
      
      // Nyari kode Akhir Gambar JPEG (0xFF 0xD9)
      if (prevByte == 0xFF && c == 0xD9) {
        // BANTING KE LAYAR IPS!
        TJpgDec.drawJpg(0, 0, frameBuffer, bufIdx);
        
        // Reset buat nangkep frame gambar selanjutnya
        isFrame = false;
        bufIdx = 0;
      } 
      // Kalo ukuran gambar ternyata lebih dari 30KB (kebesaran), buang!
      else if (bufIdx >= sizeof(frameBuffer)) {
        isFrame = false;
        bufIdx = 0;
      }
    }
    prevByte = c; // Inget byte sebelumnya
  }
}
