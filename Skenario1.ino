#include <WiFi.h>
#include <HTTPClient.h>
#include "DHTesp.h"
#include <time.h>

// =================================
// 1. KONFIGURASI SENSOR
// =================================

const int DHT_PIN = 15;
DHTesp dhtSensor;

// =================================
// 2. KONFIGURASI WI-FI
// =================================

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// =================================
// 3. KONFIGURASI SERVER HTTP
// =================================

const char* serverUrl = "http://httpbin.org/post";

// =================================
// 4. KONFIGURASI PENGUJIAN
// =================================

const int jumlahPesan = 100;
const int targetPayload = 20;

const unsigned long interval = 2000;
const unsigned long timeoutHTTP = 5000;

// =================================
// 5. KONFIGURASI WAKTU NTP
// =================================

const char* ntpServer = "pool.ntp.org";

const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// =================================
// 6. VARIABEL PENGUJIAN
// =================================

unsigned long waktuMulaiEksperimen;
unsigned long waktuSelesaiEksperimen;

int berhasil = 0;
int gagal = 0;

String waktuMulaiAbsolut;

// =================================
// 7. FUNGSI WAKTU WIB
// =================================

String waktuWIB() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 2000)) {
    return "WAKTU_TIDAK_TERSEDIA";
  }

  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%S",
    &timeinfo
  );

  return String(buffer) + "+07:00";
}

// =================================
// 8. SETUP
// =================================

void setup() {

  Serial.begin(115200);

  // Inisialisasi sensor DHT22
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  // Menghubungkan Wi-Fi
  Serial.println();
  Serial.println("Menghubungkan ke Wi-Fi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi berhasil terhubung!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Sinkronisasi waktu NTP
  Serial.println();
  Serial.println("Sinkronisasi waktu NTP...");

  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  struct tm timeinfo;
  bool waktuTersinkron = false;

  for (int i = 0; i < 20; i++) {

    if (getLocalTime(&timeinfo)) {
      waktuTersinkron = true;
      break;
    }

    Serial.println("Menunggu waktu NTP...");
    delay(1000);
  }

  if (!waktuTersinkron) {
    Serial.println("Gagal mendapatkan waktu NTP.");
    Serial.println("Pengujian dibatalkan.");

    while (true) {
      delay(1000);
    }
  }

  // Mencatat waktu mulai
  waktuMulaiAbsolut = waktuWIB();

  // Metadata eksperimen
  Serial.println();
  Serial.println("===== METADATA EKSPERIMEN =====");

  Serial.println("Skenario       : S1");
  Serial.println("Protokol       : HTTP POST");
  Serial.println("Payload target : 20 byte");
  Serial.println("Target pesan   : 100");
  Serial.println("Interval       : 2 detik");
  Serial.println("Timeout        : 5 detik");
  Serial.println("Perangkat      : ESP32");
  Serial.println("Sensor         : DHT22");
  Serial.println("Pin DATA       : GPIO 15");
  Serial.println("Zona waktu     : WIB (UTC+7)");

  Serial.print("Waktu mulai    : ");
  Serial.println(waktuMulaiAbsolut);

  Serial.println("==============================");

  // Header CSV
  Serial.println();

  Serial.println(
    "skenario,seq,payload_byte,waktu_absolut_wib,"
    "t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm"
  );

  waktuMulaiEksperimen = millis();
}

// =================================
// 9. LOOP PENGUJIAN HTTP POST
// =================================

void loop() {

  for (int seq = 1; seq <= jumlahPesan; seq++) {

    // Mencatat awal percobaan
    unsigned long awalInterval = millis();

    // Membaca sensor DHT22
    TempAndHumidity data =
      dhtSensor.getTempAndHumidity();

    // Memeriksa hasil pembacaan sensor
    if (isnan(data.temperature) ||
        isnan(data.humidity)) {

      Serial.print("S1,");
      Serial.print(seq);
      Serial.print(",0,");
      Serial.print(waktuWIB());
      Serial.println(",0,0,0,GAGAL_SENSOR,0");

      gagal++;

    } else {

      // Membuat payload tepat 20 byte
      char bufferPayload[32];

      snprintf(
        bufferPayload,
        sizeof(bufferPayload),
        "S1,%03d,%05.1f,%05.1fAA",
        seq,
        data.temperature,
        data.humidity
      );

      String payload = String(bufferPayload);

      int payloadByte = payload.length();

      // Memastikan ukuran payload tepat 20 byte
      if (payloadByte != targetPayload) {

        Serial.print("S1,");
        Serial.print(seq);
        Serial.print(",");
        Serial.print(payloadByte);
        Serial.print(",");
        Serial.print(waktuWIB());
        Serial.println(",0,0,0,GAGAL_PAYLOAD,0");

        gagal++;

      } else {

        // Membuat koneksi HTTP
        WiFiClient client;
        HTTPClient http;

        http.setTimeout(timeoutHTTP);

        if (http.begin(client, serverUrl)) {

          http.addHeader(
            "Content-Type",
            "text/plain"
          );

          http.addHeader(
            "Connection",
            "close"
          );

          // Mencatat waktu absolut dan waktu kirim
          String timestamp = waktuWIB();

          unsigned long tKirim = millis();

          // Mengirim HTTP POST
          int httpCode = http.POST(payload);

          // Mencatat waktu setelah respons diterima
          unsigned long tTerima = millis();

          // Menghitung RTT
          unsigned long rtt = tTerima - tKirim;

          // Menentukan status pengiriman
          String status;

          if (httpCode >= 200 && httpCode < 300) {

            status = "BERHASIL";
            berhasil++;

          } else {

            status = "GAGAL";
            gagal++;
          }

          // Mencatat hasil dalam format CSV
          Serial.print("S1,");
          Serial.print(seq);
          Serial.print(",");
          Serial.print(payloadByte);
          Serial.print(",");
          Serial.print(timestamp);
          Serial.print(",");
          Serial.print(tKirim);
          Serial.print(",");
          Serial.print(tTerima);
          Serial.print(",");
          Serial.print(rtt);
          Serial.print(",");
          Serial.print(status);
          Serial.print(",");
          Serial.println(WiFi.RSSI());

          http.end();

        } else {

          // Jika koneksi HTTP gagal dimulai
          gagal++;

          Serial.print("S1,");
          Serial.print(seq);
          Serial.print(",");
          Serial.print(payloadByte);
          Serial.print(",");
          Serial.print(waktuWIB());
          Serial.println(",0,0,0,GAGAL,0");
        }
      }
    }

    // Menjaga interval awal pengiriman 2 detik
    if (seq < jumlahPesan) {

      unsigned long durasi =
        millis() - awalInterval;

      if (durasi < interval) {
        delay(interval - durasi);
      }
    }
  }

  // =================================
  // 10. HASIL AKHIR PENGUJIAN
  // =================================

  waktuSelesaiEksperimen = millis();

  Serial.println();
  Serial.println("===== HASIL AKHIR S1 =====");

  Serial.print("Waktu mulai    : ");
  Serial.println(waktuMulaiAbsolut);

  Serial.print("Waktu selesai  : ");
  Serial.println(waktuWIB());

  Serial.print("Durasi (ms)    : ");
  Serial.println(
    waktuSelesaiEksperimen -
    waktuMulaiEksperimen
  );

  Serial.print("Target pesan   : ");
  Serial.println(jumlahPesan);

  Serial.print("Berhasil       : ");
  Serial.println(berhasil);

  Serial.print("Gagal          : ");
  Serial.println(gagal);

  Serial.println("Pengujian S1 selesai!");

  // Menghentikan pengiriman
  while (true) {
    delay(1000);
  }
}