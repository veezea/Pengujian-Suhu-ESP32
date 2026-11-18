
/*
 * BAGIAN A - Monitoring Suhu dan Kelembapan (DHT11) -> ThingSpeak
 *
 * Perangkat : ESP32 + DHT11
 * Pengiriman: WiFi menggunakan HTTP POST
 * Interval  : 30 detik
 * Waktu     : 19.00 - 21.00 WIB
 *
 * ThingSpeak Field:
 * Field 1 = Suhu (C)
 * Field 2 = Kelembapan (%)
 * Field 3 = RSSI (dBm)
 * Field 4 = Nomor Urut
 *
 * Board: ESP32 Dev Module
 */

// ================= LIBRARY =================
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <DHT.h>

// ============ KONFIGURASI WIFI =============
// Ganti sesuai jaringan WiFi yang digunakan
const char* WIFI_SSID = "Nama WiFi";
const char* WIFI_PASS = "Password WiFi";

// ========== KONFIGURASI THINGSPEAK =========
// Masukkan Write API Key dari channel ThingSpeak
const char* TS_WRITE_KEY = "Write API Key ThingSpeak";

// ============ KONFIGURASI SENSOR ===========
#define DHT_PIN  4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// ============ KONFIGURASI SISTEM ===========
const uint32_t INTERVAL_MS = 30000; // 30 detik

const int WINDOW_START_MIN = 19 * 60;      
const int WINDOW_END_MIN   = 21 * 60;

const long GMT_OFFSET_S = 7 * 3600; // WIB UTC+7

const char* TS_URL = "http://api.thingspeak.com/update";

// ============ VARIABEL SISTEM ==============
uint32_t lastSend = 0;
bool prevInWindow = false;

uint32_t seqNo = 0;
uint32_t okCount = 0;
uint32_t failCount = 0;
uint32_t dhtFailCount = 0;


// ============ KONEKSI WIFI =================
void connectWiFi(uint32_t timeoutMs) {

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi: menyambung");

  uint32_t t0 = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - t0 < timeoutMs) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.printf(
      " OK, IP=%s RSSI=%d dBm\n",
      WiFi.localIP().toString().c_str(),
      WiFi.RSSI()
    );

  } else {

    Serial.println(" GAGAL (akan dicoba lagi)");

  }
}


// ============ PEMBACAAN SENSOR =============
bool readDHT(float &t, float &h) {

  for (int i = 0; i < 3; i++) {

    h = dht.readHumidity(true);
    t = dht.readTemperature(false, false);

    if (!isnan(t) && !isnan(h)) {
      return true;
    }

    delay(2100);
  }

  return false;
}


// ============ KIRIM DATA THINGSPEAK ========
bool sendToThingSpeak(float t, float h, int rssi) {

  WiFiClient client;
  HTTPClient http;

  http.begin(client, TS_URL);
  http.setTimeout(8000);

  http.addHeader(
    "Content-Type",
    "application/x-www-form-urlencoded"
  );

  String body =
    "api_key=" + String(TS_WRITE_KEY) +
    "&field1=" + String(t, 1) +
    "&field2=" + String(h, 1) +
    "&field3=" + String(rssi) +
    "&field4=" + String(seqNo);

  int code = http.POST(body);

  String resp = (code > 0)
                ? http.getString()
                : String("");

  http.end();

  // ThingSpeak mengembalikan nomor entri jika berhasil
  return (code == 200 && resp.toInt() > 0);
}


// ============ SETUP ========================
void setup() {

  Serial.begin(115200);
  delay(500);

  // Inisialisasi sensor DHT11
  dht.begin();

  // Menghubungkan ESP32 ke WiFi
  connectWiFi(20000);

  // Sinkronisasi waktu WIB menggunakan NTP
  configTime(
    GMT_OFFSET_S,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  struct tm ti;

  if (getLocalTime(&ti, 10000)) {

    Serial.printf(
      "Waktu NTP OK: %02d:%02d:%02d WIB\n",
      ti.tm_hour,
      ti.tm_min,
      ti.tm_sec
    );

  } else {

    Serial.println(
      "Waktu NTP belum tersinkron (akan dicoba terus)"
    );

  }
}


// ============ LOOP UTAMA ===================
void loop() {

  // Memeriksa koneksi WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi(10000);
  }

  // Membaca waktu dari NTP
  struct tm ti;

  if (!getLocalTime(&ti, 500)) {
    delay(1000);
    return;
  }

  // Menghitung waktu dalam menit
  int nowMin = ti.tm_hour * 60 + ti.tm_min;

  // Memeriksa jendela waktu pengiriman
  bool inWindow =
    (nowMin >= WINDOW_START_MIN &&
     nowMin < WINDOW_END_MIN);

  // Penanda awal pengiriman
  if (inWindow && !prevInWindow) {
    Serial.println("== Jendela rekam DIMULAI ==");
  }

  // Penanda akhir pengiriman
  if (!inWindow && prevInWindow) {

    Serial.printf(
      "== Jendela rekam SELESAI: "
      "ok=%u gagal_kirim=%u gagal_dht=%u ==\n",
      okCount,
      failCount,
      dhtFailCount
    );

  }

  // Pengiriman data sesuai interval
  if (inWindow &&
      (!prevInWindow ||
       millis() - lastSend >= INTERVAL_MS)) {

    lastSend = millis();

    float t, h;

    // Membaca suhu dan kelembapan
    if (readDHT(t, h)) {

      seqNo++;

      int rssi = WiFi.RSSI();

      // Mengirim data ke ThingSpeak
      bool ok = sendToThingSpeak(t, h, rssi);

      if (ok) {
        okCount++;
      } else {
        failCount++;
      }

      // Menampilkan hasil pembacaan di Serial Monitor
      Serial.printf(
        "[%02d:%02d:%02d] "
        "T=%.1fC H=%.1f%% RSSI=%d seq=%u -> %s\n",
        ti.tm_hour,
        ti.tm_min,
        ti.tm_sec,
        t,
        h,
        rssi,
        seqNo,
        ok ? "OK" : "GAGAL"
      );

    } else {

      dhtFailCount++;

      Serial.println(
        "DHT11 gagal dibaca "
        "(cek kabel/pull-up); sampel dilewati"
      );

    }
  }

  prevInWindow = inWindow;

  delay(200);
}