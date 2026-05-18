#include <WiFi.h>
#include <WiFiClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>

// --- 1. KONFIGURASI WIFI HOTSPOT HP LU ---
const char* ssid = "NOT MASTAH";     
const char* password = "yangbrorasakan"; 

// --- 2. VARIABEL IP & PORT CUSTOM (AKAN DIISI LEWAT SERIAL) ---
String host = ""; 
int port = 8080;

// --- 3. PIN LAYAR IPS ESP32-S3 LU ---
#define TFT_CS    10  
#define TFT_DC    9  
#define TFT_RST   8
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Buffer digedein jadi 150KB biar gak crash pas kirim kualitas 100%
uint8_t frameBuffer[150000]; 
WiFiClient client;

// Callback buat nampilin gambar ke layar
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return true;
}

void setup() {
  delay(3000);
  // Paksa pake HW Serial bawaan USB S3
  Serial.begin(115200); 
  
  // Kasih waktu 3 detik buat USB sinkronisasi
  long waktuAwal = millis();
  while (!Serial && (millis() - waktuAwal < 3000)) {
    delay(10);
  }
  
  Serial.println("\n[OK] SISTEM SIAP!");

  // Setup Layar
  tft.init(135, 240);
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
  Serial.println("\n[OK] WiFi Berhasil Terhubung!");
  delay(1000);
  
  // Tampilan nunggu input di Layar IPS
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 30);
  tft.print("Input IP & Port");
  tft.setCursor(10, 60);
  tft.print("di Serial...");

  // === FITUR CUSTOM IP LEWAT SERIAL MONITOR ===
  
  // 1. Minta IP Address
  Serial.println("\n=====================================");
  Serial.println("Masukkan IP APK Screen Stream Lu:");
  Serial.println("Contoh: 192.168.137.193");
  Serial.println("=====================================");
  
  while (Serial.available() > 0) Serial.read(); 
  while (!Serial.available()) {
    delay(10); 
  }
  host = Serial.readStringUntil('\n');
  host.trim(); 
  
  Serial.print("-> IP Diterima: ");
  Serial.println(host);

  // 2. Minta Port
  Serial.println("\n=====================================");
  Serial.println("Masukkan Port-nya (Ketik angkanya aja):");
  Serial.println("Contoh: 8080");
  =====================================");
  
  while (Serial.available() > 0) Serial.read(); 
  while (!Serial.available()) {
    delay(10); 
  }
  String portStr = Serial.readStringUntil('\n');
  portStr.trim();
  port = portStr.toInt(); 
  
  Serial.print("-> Port Diterima: ");
  Serial.println(port);
  
  Serial.println("\n[GAS] Mencoba Koneksi Streaming Gacor...");
  
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  if (!client.connected()) {
    if (client.connect(host.c_str(), port)) { 
      client.print(String("GET /stream.mjpeg HTTP/1.1\r\n") +
                   "Host: " + host + "\r\n" +
                   "Connection: keep-alive\r\n\r\n");
    } else {
      delay(100); 
      return;
    }
  }

  // --- MESIN PENYEDOT CHUNKS ---
  static uint32_t bufIdx = 0;
  static bool isFrame = false;
  static uint8_t prevByte = 0;
  
  #define CHUNK_SIZE 4096
  static uint8_t chunkBuffer[CHUNK_SIZE]; 

  int availableBytes = client.available();
  if (availableBytes > 0) {
    int bytesToRead = min(availableBytes, CHUNK_SIZE);
    int readBytes = client.read(chunkBuffer, bytesToRead);

    for (int i = 0; i < readBytes; i++) {
      uint8_t c = chunkBuffer[i];

      if (!isFrame) {
        if (prevByte == 0xFF && c == 0xD8) {
          isFrame = true;
          bufIdx = 0;
          frameBuffer[bufIdx++] = 0xFF;
          frameBuffer[bufIdx++] = 0xD8;
        }
      } else {
        if (bufIdx < 150000) { 
          frameBuffer[bufIdx++] = c;
        } else {
          isFrame = false;
          bufIdx = 0;
        }

        if (prevByte == 0xFF && c == 0xD9 && isFrame) {
          
          // ====================================================
          // 🔥 DISINI LOGIKA DEWA AUTO-ROTATENYA GUE DEKLARE 🔥
          // ====================================================
          uint16_t jpg_w = 0, jpg_h = 0;
          // Ngintip isi kepala file JPEG buat nyari tahu ukuran aslinya
          TJpgDec.getJpgSize(&jpg_w, &jpg_h, frameBuffer, bufIdx);
          
          static uint8_t rotasiLama = 99;
          uint8_t rotasiTarget = 1; // Default miring (Landscape)
          
          if (jpg_w < jpg_h) {
            rotasiTarget = 0; // HP Tegak -> Layar IPS ikut diputar Tegak (Portrait)
          } else {
            rotasiTarget = 1; // HP Miring -> Layar IPS ikut diputar Miring (Landscape)
          }
          
          // Kalau posisi HP berubah, reset arah layar & bersihin sisa gambar lama
          if (rotasiTarget != rotasiLama) {
            tft.setRotation(rotasiTarget);
            tft.fillScreen(ST77XX_BLACK); 
            rotasiLama = rotasiTarget;
          }
          // ====================================================

          TJpgDec.setJpgScale(1); 
          
          // BONUS OPTIMASI: Jalur kabel dikunci biar render anime-nya makin kebut rata kanan
          tft.startWrite();
          TJpgDec.drawJpg(0, 0, frameBuffer, bufIdx);
          tft.endWrite();
          
          isFrame = false;
          bufIdx = 0;
        }
      }
      prevByte = c;
    }
  }
}
