#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// --- KONFIGURASI PIN & BOT ---
#define RELAY_PIN 4 // Sesuaikan dengan pin relay lo
#define LED_BUILTIN 48 // Indikator

const String BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN"; 

// --- OBJEK GLOBAL ---
Preferences preferences;
AsyncWebServer server(80);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// --- VARIABEL STATE ---
String ap_ssid = "DETONATOR";
String ap_pass = "12345678";
String sta_ssid = "";
String sta_pass = "";
bool relayState = false;

// Waktu polling Telegram
unsigned long lastTimeBotRan = 0;
const unsigned long botRequestDelay = 1000; 

// ==============================================================================
// HTML + CSS + JS (PRO MINIMALIST UI)
// ==============================================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Control Panel</title>
  <style>
    :root {
      --bg: #121212; --card: #1e1e1e; --text: #e0e0e0;
      --accent: #10b981; --accent-hover: #059669;
      --border: #333; --danger: #ef4444;
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background-color: var(--bg); color: var(--text);
      display: flex; justify-content: center; padding: 20px; margin: 0;
    }
    .container { width: 100%; max-width: 400px; }
    h2 { text-align: center; font-weight: 500; margin-bottom: 30px; letter-spacing: 1px; }
    .card {
      background: var(--card); border: 1px solid var(--border);
      border-radius: 12px; padding: 20px; margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.3);
    }
    .card h3 { margin-top: 0; font-size: 16px; color: #9ca3af; border-bottom: 1px solid var(--border); padding-bottom: 10px; }
    .form-group { margin-bottom: 15px; }
    label { display: block; font-size: 14px; margin-bottom: 5px; color: #ccc; }
    input[type="text"], input[type="password"] {
      width: 100%; padding: 10px; border-radius: 6px;
      border: 1px solid var(--border); background: var(--bg); color: var(--text);
      box-sizing: border-box; font-size: 14px; transition: border 0.3s;
    }
    input:focus { border-color: var(--accent); outline: none; }
    button {
      width: 100%; padding: 12px; border: none; border-radius: 6px;
      background: var(--accent); color: #fff; font-size: 15px; font-weight: 600;
      cursor: pointer; transition: background 0.3s;
    }
    button:hover { background: var(--accent-hover); }
    
    /* Toggle Switch Presisi */
    .toggle-wrapper { display: flex; justify-content: space-between; align-items: center; }
    .switch { position: relative; display: inline-block; width: 50px; height: 28px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
      background-color: var(--border); transition: .4s; border-radius: 34px;
    }
    .slider:before {
      position: absolute; content: ""; height: 20px; width: 20px;
      left: 4px; bottom: 4px; background-color: white;
      transition: .4s; border-radius: 50%; box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    input:checked + .slider { background-color: var(--accent); }
    input:checked + .slider:before { transform: translateX(22px); }
    
    .status-toast {
      position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
      background: #333; color: #fff; padding: 10px 20px; border-radius: 20px;
      font-size: 14px; opacity: 0; transition: opacity 0.3s; pointer-events: none;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>ESP32 SYSTEM</h2>

    <div class="card">
      <h3>Device Control</h3>
      <div class="toggle-wrapper">
        <span>Relay Power</span>
        <label class="switch">
          <input type="checkbox" id="relayToggle" onchange="toggleRelay()">
          <span class="slider"></span>
        </label>
      </div>
    </div>

    <div class="card">
      <h3>Local AP Settings (8.8.8.8)</h3>
      <div class="form-group">
        <label>New AP Name</label>
        <input type="text" id="ap_ssid" placeholder="ESP32_Network">
      </div>
      <div class="form-group">
        <label>New AP Password</label>
        <input type="password" id="ap_pass" placeholder="Min 8 chars">
      </div>
      <button onclick="saveAP()">Save & Restart AP</button>
    </div>

    <div class="card">
      <h3>Internet Connection (For Bot)</h3>
      <div class="form-group">
        <label>Router WiFi Name</label>
        <input type="text" id="sta_ssid" placeholder="Home_WiFi">
      </div>
      <div class="form-group">
        <label>Router Password</label>
        <input type="password" id="sta_pass" placeholder="Password">
      </div>
      <button onclick="saveSTA()">Connect</button>
    </div>
  </div>

  <div id="toast" class="status-toast">Saved!</div>

  <script>
    function showToast(msg) {
      const toast = document.getElementById('toast');
      toast.innerText = msg; toast.style.opacity = '1';
      setTimeout(() => toast.style.opacity = '0', 3000);
    }

    // Ambil status relay saat pertama load
    fetch('/status').then(r => r.text()).then(state => {
      document.getElementById('relayToggle').checked = (state === '1');
    });

    function toggleRelay() {
      fetch('/toggle').then(r => r.text()).then(res => {
        showToast('Relay ' + (res === '1' ? 'ON' : 'OFF'));
      });
    }

    function saveAP() {
      const s = document.getElementById('ap_ssid').value;
      const p = document.getElementById('ap_pass').value;
      if(s && p.length >= 8) {
        fetch(`/setap?ssid=${encodeURIComponent(s)}&pass=${encodeURIComponent(p)}`)
          .then(() => showToast('AP Updated! Rebooting...'));
      } else { showToast('Invalid input! Pass min 8 chars.'); }
    }

    function saveSTA() {
      const s = document.getElementById('sta_ssid').value;
      const p = document.getElementById('sta_pass').value;
      if(s) {
        fetch(`/setsta?ssid=${encodeURIComponent(s)}&pass=${encodeURIComponent(p)}`)
          .then(() => showToast('Connecting...'));
      } else { showToast('SSID cannot be empty'); }
    }
  </script>
</body>
</html>
)rawliteral";

// ==============================================================================
// TELEGRAM BOT HANDLER
// ==============================================================================
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (text == "/start") {
      String welcome = "🤖 *ESP32 Control Panel*\n\n";
      welcome += "System Online. Pilih menu di bawah ini:";
      
      // Keyboard Menu Presisi
      String keyboardJson = "[[\"💥 Toggle Relay\"], [\"⚙️ Ubah AP ESP32\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, welcome, "Markdown", keyboardJson, true);
    }
    else if (text == "💥 Toggle Relay") {
      relayState = !relayState;
      
      // Logic Active-Low dengan trik Input/Output
      if (relayState) {
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, LOW); // ON (Tarik ke GND)
      } else {
        pinMode(RELAY_PIN, INPUT);    // OFF (Biarkan mengambang/High-Z)
      }
      
      String status = relayState ? "🟢 Relay ON" : "🔴 Relay OFF";
      bot.sendMessage(chat_id, status, "");
    }

    else if (text == "⚙️ Ubah AP ESP32") {
      String msg = "Untuk mengubah WiFi lokal ESP32, kirim pesan dengan format:\n\n";
      msg += "`/setap Namawifi Passwordbaru`\n\n";
      msg += "*(Password minimal 8 karakter)*";
      bot.sendMessage(chat_id, msg, "Markdown");
    }
    else if (text.startsWith("/setap ")) {
      // Parsing logic: /setap SSID PASS
      int firstSpace = text.indexOf(" ");
      int secondSpace = text.indexOf(" ", firstSpace + 1);
      
      if (firstSpace > 0 && secondSpace > 0) {
        String new_ssid = text.substring(firstSpace + 1, secondSpace);
        String new_pass = text.substring(secondSpace + 1);
        
        if (new_pass.length() >= 8) {
          preferences.begin("config", false);
          preferences.putString("apssid", new_ssid);
          preferences.putString("appass", new_pass);
          preferences.end();
          
          bot.sendMessage(chat_id, "✅ AP berhasil diubah ke: " + new_ssid + "\nSistem akan restart dalam 3 detik...", "");
          delay(3000);
          ESP.restart();
        } else {
          bot.sendMessage(chat_id, "❌ Gagal: Password minimal 8 karakter!", "");
        }
      } else {
        bot.sendMessage(chat_id, "❌ Format salah! Gunakan: /setap NamaWifi Password", "");
      }
    }
  }
}

// ==============================================================================
// MAIN SETUP & LOOP
// ==============================================================================
void setup() {
  Serial.begin(115200);
    // Pengaman boot: Set sebagai INPUT biar pin High-Impedance (Relay OFF)
  pinMode(RELAY_PIN, INPUT); 
  pinMode(LED_BUILTIN, OUTPUT);


  // 1. Muat Konfigurasi dari NVS (Memori Permanen)
  preferences.begin("config", false);
  ap_ssid = preferences.getString("apssid", "DETONATOR");
  ap_pass = preferences.getString("appass", "12345678");
  sta_ssid = preferences.getString("stassid", "");
  sta_pass = preferences.getString("stapass", "");
  preferences.end();

  // 2. Setup Access Point (Target IP: 8.8.8.8)
  WiFi.mode(WIFI_AP_STA);
  IPAddress local_IP(8, 8, 8, 8);
  IPAddress gateway(8, 8, 8, 8);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
  Serial.println("\n--- AP STARTED ---");
  Serial.print("AP IP Address: "); Serial.println(WiFi.softAPIP());

  // 3. Setup Station (Connect to Internet if configured)
  if (sta_ssid != "") {
    Serial.println("Connecting to WiFi: " + sta_ssid);
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  }

  // 4. Setup Client Secure untuk Telegram
  secured_client.setInsecure(); // Agar tidak perlu validasi sertifikat SSL rumit

  // 5. Routing Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(relayState ? 1 : 0));
  });

    server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    relayState = !relayState;
    
    // Logic Active-Low dengan trik Input/Output
    if (relayState) {
      pinMode(RELAY_PIN, OUTPUT);
      digitalWrite(RELAY_PIN, LOW); // ON
    } else {
      pinMode(RELAY_PIN, INPUT);    // OFF
    }
    
    request->send(200, "text/plain", String(relayState ? 1 : 0));
  });

  server.on("/setap", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("ssid") && request->hasParam("pass")) {
      String newSsid = request->getParam("ssid")->value();
      String newPass = request->getParam("pass")->value();
      
      preferences.begin("config", false);
      preferences.putString("apssid", newSsid);
      preferences.putString("appass", newPass);
      preferences.end();
      
      request->send(200, "text/plain", "OK");
      delay(1000); ESP.restart(); // Restart untuk apply AP baru
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  server.on("/setsta", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("ssid") && request->hasParam("pass")) {
      String newSsid = request->getParam("ssid")->value();
      String newPass = request->getParam("pass")->value();
      
      preferences.begin("config", false);
      preferences.putString("stassid", newSsid);
      preferences.putString("stapass", newPass);
      preferences.end();
      
      request->send(200, "text/plain", "OK");
      delay(1000); ESP.restart(); // Restart untuk connect STA
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  server.begin();
  Serial.println("Web Server Running.");
}

void loop() {
  // Hanya jalankan bot Telegram jika ESP tersambung ke internet (STA Mode)
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastTimeBotRan > botRequestDelay) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      while (numNewMessages) {
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotRan = millis();
    }
  }
}
