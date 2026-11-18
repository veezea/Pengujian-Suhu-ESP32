#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include "DHTesp.h"
#include <time.h>

// ===============================
// KONFIGURASI WIFI
// ===============================
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ===============================
// KONFIGURASI SENSOR
// ===============================
const int DHT_PIN = 15;
DHTesp dht;

// ===============================
// KONFIGURASI MQTT
// ===============================
// Gunakan broker yang berhasil digunakan pada S4
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

String clientId;
String topic;
String expectedPayload;

// ===============================
// KONFIGURASI PENGUJIAN
// ===============================
const int JUMLAH_PESAN = 100;
const int TARGET_PAYLOAD = 20;

const unsigned long INTERVAL = 2000;
const unsigned long TIMEOUT = 5000;

int berhasil = 0;
int gagal = 0;

bool echoDiterima = false;
unsigned long waktuTerima = 0;

String waktuMulaiAbsolut;
unsigned long waktuMulai;

// ===============================
// WAKTU WIB
// ===============================
String waktuWIB() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "WAKTU_TIDAK_TERSEDIA";
  }

  char buffer[30];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%S",
    &timeinfo
  );

  return String(buffer) + "+07:00";
}

// ===============================
// MEMBUAT PAYLOAD 20 BYTE
// ===============================
String buatPayload(int seq, float suhu, float kelembapan) {

  String payload =
    "S5," + String(seq) +
    "," + String(suhu, 1) +
    "," + String(kelembapan, 1) +
    ",";

  while (payload.length() < TARGET_PAYLOAD) {
    payload += "A";
  }

  return payload;
}

// ===============================
// CALLBACK PESAN MQTT
// ===============================
void onMqttMessage(int messageSize) {

  String receivedTopic = mqttClient.messageTopic();
  String pesan = "";

  while (mqttClient.available()) {
    pesan += (char)mqttClient.read();
  }

  if (
    receivedTopic == topic &&
    pesan == expectedPayload &&
    !echoDiterima
  ) {
    waktuTerima = millis();
    echoDiterima = true;
  }
}

// ===============================
// KONEKSI MQTT
// ===============================
bool koneksiMQTT() {

  int percobaan = 0;

  while (!mqttClient.connected() && percobaan < 5) {

    Serial.println("Menghubungkan ke broker MQTT...");

    if (mqttClient.connect(mqttServer, mqttPort)) {

      Serial.println("MQTT berhasil terhubung.");

      if (mqttClient.subscribe(topic, 1)) {
        Serial.println("Subscribe QoS 1 berhasil.");
        return true;
      }

      Serial.println("Subscribe gagal.");

    } else {

      Serial.print("Koneksi MQTT gagal. Kode: ");
      Serial.println(mqttClient.connectError());

    }

    percobaan++;
    delay(2000);
  }

  return mqttClient.connected();
}

// ===============================
// SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Koneksi Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  Serial.print("Menghubungkan Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi terhubung.");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  // Sinkronisasi waktu WIB
  configTime(7 * 3600, 0, "pool.ntp.org");

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 15000)) {
    Serial.println("Gagal mendapatkan waktu NTP.");

    while (true) {
      delay(1000);
    }
  }

  // Identitas perangkat dan topik
  clientId = "ESP32_S5_" +
    String((uint32_t)ESP.getEfuseMac(), HEX);

  topic = "josjis/s5/echo/" + clientId;

  mqttClient.setId(clientId);
  mqttClient.setKeepAliveInterval(60000);
  mqttClient.setConnectionTimeout(5000);

  mqttClient.onMessage(onMqttMessage);

  // Koneksi broker MQTT
  if (!koneksiMQTT()) {

    Serial.println("MQTT tidak berhasil terhubung.");

    while (true) {
      delay(1000);
    }
  }

  // Metadata
  waktuMulaiAbsolut = waktuWIB();
  waktuMulai = millis();

  Serial.println("\n=== METADATA S5 ===");
  Serial.println("Skenario: S5");
  Serial.println("Protokol: MQTT QoS 1");
  Serial.println("Target payload: 20 byte");
  Serial.println("Target pesan: 100");
  Serial.println("Interval: 2 detik");
  Serial.println("Timeout: 5000 ms");
  Serial.println("Perangkat: ESP32");
  Serial.println("Sensor: DHT22");
  Serial.println("Broker: " + String(mqttServer));
  Serial.println("Port: 1883");
  Serial.println("Waktu mulai WIB: " + waktuMulaiAbsolut);

  Serial.println("\n=== DATA CSV ===");

  Serial.println(
    "skenario,seq,payload_byte,waktu_absolut_wib,"
    "t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm"
  );
}

// ===============================
// LOOP PENGUJIAN
// ===============================
void loop() {

  static bool pengujianSelesai = false;

  if (pengujianSelesai) {
    return;
  }

  unsigned long jadwalKirim = waktuMulai;

  for (int seq = 1; seq <= JUMLAH_PESAN; seq++) {

    // Menjaga interval minimal 2 detik
    while ((long)(millis() - jadwalKirim) < 0) {
      mqttClient.poll();
      delay(1);
    }

    // Memeriksa koneksi MQTT
    if (!mqttClient.connected()) {

      Serial.println("MQTT terputus.");

      if (!koneksiMQTT()) {
        Serial.println("Pengujian dihentikan.");
        gagal += JUMLAH_PESAN - seq + 1;
        break;
      }
    }

    mqttClient.poll();

    // Membaca sensor
    TempAndHumidity data = dht.getTempAndHumidity();

    if (
      isnan(data.temperature) ||
      isnan(data.humidity)
    ) {

      Serial.println(
        "S5," + String(seq) +
        ",0," + waktuWIB() +
        ",0,0,0,SENSOR_GAGAL," +
        String(WiFi.RSSI())
      );

      gagal++;
      jadwalKirim += INTERVAL;
      continue;
    }

    // Membuat payload
    expectedPayload = buatPayload(
      seq,
      data.temperature,
      data.humidity
    );

    int payloadByte = expectedPayload.length();

    if (payloadByte != TARGET_PAYLOAD) {

      Serial.println(
        "S5," + String(seq) +
        "," + String(payloadByte) +
        "," + waktuWIB() +
        ",0,0,0,UKURAN_PAYLOAD_SALAH," +
        String(WiFi.RSSI())
      );

      gagal++;
      jadwalKirim += INTERVAL;
      continue;
    }

    echoDiterima = false;
    waktuTerima = 0;

    String timestamp = waktuWIB();
    unsigned long tKirim = millis();

    // Memulai pengiriman MQTT QoS 1
    int mulaiKirim = mqttClient.beginMessage(
      topic,
      (unsigned long)payloadByte,
      false,
      1
    );

    if (!mulaiKirim) {

      Serial.println(
        "S5," + String(seq) +
        "," + String(payloadByte) +
        "," + timestamp +
        "," + String(tKirim) +
        ",0,0,GAGAL_PUBLISH," +
        String(WiFi.RSSI())
      );

      gagal++;

    } else {

      size_t jumlahTerkirim = mqttClient.print(expectedPayload);

      // endMessage menunggu PUBACK untuk QoS 1
      int hasilPublish = mqttClient.endMessage();

      if (
        jumlahTerkirim != payloadByte ||
        hasilPublish != 1
      ) {

        Serial.println(
          "S5," + String(seq) +
          "," + String(payloadByte) +
          "," + timestamp +
          "," + String(tKirim) +
          ",0,0,GAGAL_PUBACK," +
          String(WiFi.RSSI())
        );

        gagal++;

      } else {

        // Menunggu echo maksimal 5 detik
        while (
          !echoDiterima &&
          millis() - tKirim < TIMEOUT
        ) {

          mqttClient.poll();
          delay(1);
        }

        if (echoDiterima) {

          unsigned long rtt = waktuTerima - tKirim;

          Serial.println(
            "S5," + String(seq) +
            "," + String(payloadByte) +
            "," + timestamp +
            "," + String(tKirim) +
            "," + String(waktuTerima) +
            "," + String(rtt) +
            ",BERHASIL," +
            String(WiFi.RSSI())
          );

          berhasil++;

        } else {

          Serial.println(
            "S5," + String(seq) +
            "," + String(payloadByte) +
            "," + timestamp +
            "," + String(tKirim) +
            ",0,0,TIMEOUT," +
            String(WiFi.RSSI())
          );

          gagal++;
        }
      }
    }

    jadwalKirim += INTERVAL;
  }

  // ===============================
  // HASIL PENGUJIAN
  // ===============================
  Serial.println("\n=== HASIL PENGUJIAN S5 ===");

  Serial.println("Target pesan: " + String(JUMLAH_PESAN));
  Serial.println("Berhasil: " + String(berhasil));
  Serial.println("Gagal: " + String(gagal));

  Serial.println("Waktu mulai WIB: " + waktuMulaiAbsolut);
  Serial.println("Waktu selesai WIB: " + waktuWIB());

  Serial.println(
    "Durasi pengujian: " +
    String(millis() - waktuMulai) + " ms"
  );

  pengujianSelesai = true;
}