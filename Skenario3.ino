#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <time.h>

// Konfigurasi Wi-Fi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Konfigurasi DHT22
const int DHT_PIN = 15;
DHTesp dht;

// Konfigurasi MQTT
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

String clientId;
String topic;
String expectedPayload;

bool echoDiterima = false;
unsigned long waktuTerima = 0;

// Konfigurasi pengujian
const int JUMLAH_PESAN = 100;
const int TARGET_PAYLOAD = 20;
const unsigned long INTERVAL = 2000;
const unsigned long TIMEOUT = 5000;

int berhasil = 0;
int gagal = 0;

// Waktu absolut WIB
String waktuWIB() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "WAKTU_TIDAK_TERSEDIA";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer),
           "%Y-%m-%dT%H:%M:%S", &timeinfo);

  return String(buffer) + "+07:00";
}

// Membuat payload tepat 20 byte
String buatPayload(int seq, float suhu, float kelembapan) {
  char buffer[32];

  snprintf(buffer, sizeof(buffer),
           "S3,%04d,%05.1f,%05.1f",
           seq, suhu, kelembapan);

  return String(buffer) + "A";
}

// Callback ketika pesan MQTT diterima
void callback(char* receivedTopic,
              byte* payload,
              unsigned int length) {

  String pesan = "";

  for (unsigned int i = 0; i < length; i++) {
    pesan += (char)payload[i];
  }

  if (String(receivedTopic) == topic &&
      pesan == expectedPayload) {

    echoDiterima = true;
    waktuTerima = millis();
  }
}

// Menghubungkan ke broker MQTT
bool koneksiMQTT() {
  while (!client.connected()) {
    Serial.println("Menghubungkan ke broker MQTT...");

    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT terhubung.");

      client.subscribe(topic.c_str());
      return true;
    }

    Serial.println("Koneksi MQTT gagal.");
    delay(2000);
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Menghubungkan Wi-Fi
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi terhubung.");

  // Mengatur waktu WIB
  configTime(7 * 3600, 0, "pool.ntp.org");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 15000)) {
    Serial.println("Gagal mendapatkan waktu NTP.");
    while (true) delay(1000);
  }

  // Membuat ID dan topik unik
  clientId = "ESP32_S3_" +
             String((uint32_t)ESP.getEfuseMac(), HEX);

  topic = "josjis/s3/echo/" + clientId;

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  client.setBufferSize(256);

  if (!koneksiMQTT()) {
    Serial.println("MQTT tidak terhubung.");
    while (true) delay(1000);
  }

  // Metadata pengujian
  Serial.println("\n=== METADATA S3 ===");
  Serial.println("Skenario: S3");
  Serial.println("Protokol: MQTT QoS 0");
  Serial.println("Target payload: 20 byte");
  Serial.println("Target pesan: 100");
  Serial.println("Interval: 2 detik");
  Serial.println("Timeout: 5000 ms");
  Serial.println("Perangkat: ESP32");
  Serial.println("Sensor: DHT22");
  Serial.println("Waktu mulai WIB: " + waktuWIB());

  Serial.println("\n=== DATA CSV ===");
  Serial.println(
    "skenario,seq,payload_byte,waktu_absolut_wib,"
    "t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm"
  );
}

void loop() {
  static bool pengujianSelesai = false;

  if (pengujianSelesai) {
    return;
  }

  unsigned long waktuMulai = millis();
  unsigned long jadwalKirim = waktuMulai;

  for (int seq = 1; seq <= JUMLAH_PESAN; seq++) {

    // Menjaga jadwal pengiriman 2 detik
    while ((long)(millis() - jadwalKirim) < 0) {
      client.loop();
      delay(1);
    }

    if (!client.connected()) {
      koneksiMQTT();
    }

    client.loop();

    TempAndHumidity data = dht.getTempAndHumidity();

    if (isnan(data.temperature) || isnan(data.humidity)) {
      Serial.println(
        "S3," + String(seq) +
        ",0," + waktuWIB() +
        ",0,0,0,SENSOR_GAGAL,0"
      );

      gagal++;
      jadwalKirim += INTERVAL;
      continue;
    }

    expectedPayload = buatPayload(
      seq, data.temperature, data.humidity
    );

    int payloadByte = expectedPayload.length();

    if (payloadByte != TARGET_PAYLOAD) {
      Serial.println("Payload tidak berukuran 20 byte.");
      gagal++;
      jadwalKirim += INTERVAL;
      continue;
    }

    echoDiterima = false;
    waktuTerima = 0;

    String timestamp = waktuWIB();
    unsigned long tKirim = millis();

    bool terkirim = client.publish(
      topic.c_str(),
      expectedPayload.c_str()
    );

    if (!terkirim) {
      Serial.println(
        "S3," + String(seq) + "," +
        String(payloadByte) + "," + timestamp +
        "," + String(tKirim) + ",0,0,GAGAL_PUBLISH," +
        String(WiFi.RSSI())
      );

      gagal++;
    } else {

      // Menunggu pesan echo maksimal 5 detik
      unsigned long batasWaktu = millis();

      while (!echoDiterima &&
             millis() - batasWaktu < TIMEOUT) {

        client.loop();
        delay(1);
      }

      if (echoDiterima) {
        unsigned long rtt = waktuTerima - tKirim;

        Serial.println(
          "S3," + String(seq) + "," +
          String(payloadByte) + "," + timestamp +
          "," + String(tKirim) + "," +
          String(waktuTerima) + "," +
          String(rtt) + ",BERHASIL," +
          String(WiFi.RSSI())
        );

        berhasil++;

      } else {
        Serial.println(
          "S3," + String(seq) + "," +
          String(payloadByte) + "," + timestamp +
          "," + String(tKirim) + ",0,0,TIMEOUT," +
          String(WiFi.RSSI())
        );

        gagal++;
      }
    }

    jadwalKirim += INTERVAL;
  }

  Serial.println("\n=== HASIL PENGUJIAN S3 ===");
  Serial.println("Target pesan: " + String(JUMLAH_PESAN));
  Serial.println("Berhasil: " + String(berhasil));
  Serial.println("Gagal: " + String(gagal));
  Serial.println("Waktu selesai WIB: " + waktuWIB());
  Serial.println(
    "Durasi pengujian: " +
    String(millis() - waktuMulai) + " ms"
  );

  pengujianSelesai = true;
}
