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
// KONFIGURASI MQTT
// ===============================
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// ===============================
// KONFIGURASI DHT22
// ===============================
DHTesp dht;
const int DHT_PIN = 15;

// ===============================
// KONFIGURASI PENGUJIAN S6
// ===============================
const int TARGET_PESAN = 100;
const int TARGET_PAYLOAD = 500;

const unsigned long INTERVAL_MS = 2000;
const unsigned long TIMEOUT_MS = 5000;

// Identitas perangkat dan topik MQTT
String clientId;
String topic;

// Variabel pengiriman dan penerimaan
String expectedPayload = "";

bool waitingEcho = false;
bool echoReceived = false;
bool echoMatches = false;

unsigned long tKirim = 0;
unsigned long tTerima = 0;
unsigned long lastSend = 0;

// ===============================
// FUNGSI WAKTU WIB
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
// FUNGSI MEMBUAT PAYLOAD 500 BYTE
// ===============================
String buatPayload(
  int seq,
  float suhu,
  float kelembapan
) {
  String payload =
    "S6," +
    String(seq) +
    "," +
    String(suhu, 1) +
    "," +
    String(kelembapan, 1) +
    ",data=";

  // Menambahkan karakter A sampai tepat 500 byte
  while (payload.length() < TARGET_PAYLOAD) {
    payload += "A";
  }

  // Memastikan ukuran tidak melebihi 500 byte
  if (payload.length() > TARGET_PAYLOAD) {
    payload = payload.substring(0, TARGET_PAYLOAD);
  }

  return payload;
}

// ===============================
// CALLBACK PESAN MQTT
// ===============================
void onMqttMessage(int messageSize) {
  String incomingTopic = mqttClient.messageTopic();
  String incomingPayload = "";

  while (mqttClient.available()) {
    incomingPayload += (char)mqttClient.read();
  }

  // Memeriksa pesan echo dari topik sendiri
  if (waitingEcho && incomingTopic == topic) {
    tTerima = millis();
    echoReceived = true;

    if (incomingPayload == expectedPayload) {
      echoMatches = true;
    } else {
      echoMatches = false;
    }
  }
}

// ===============================
// KONEKSI WIFI
// ===============================
void koneksiWiFi() {
  Serial.println("# Menghubungkan ke WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("# WiFi terhubung");
  Serial.print("# IP Address: ");
  Serial.println(WiFi.localIP());
}

// ===============================
// KONEKSI MQTT
// ===============================
bool koneksiMQTT() {
  Serial.println("# Menghubungkan ke broker MQTT...");

  for (int percobaan = 1; percobaan <= 5; percobaan++) {
    Serial.print("# Percobaan MQTT ke-");
    Serial.println(percobaan);

    if (mqttClient.connect(mqttServer, mqttPort)) {
      Serial.println("# MQTT berhasil terhubung");

      if (mqttClient.subscribe(topic.c_str(), 1)) {
        Serial.println("# Berhasil subscribe QoS 1");
        return true;
      } else {
        Serial.println("# Subscribe gagal");
        return false;
      }
    }

    Serial.print("# MQTT gagal, error: ");
    Serial.println(mqttClient.connectError());

    delay(2000);
  }

  return false;
}

// ===============================
// SETUP
// ===============================
void setup() {
  Serial.begin(115200);

  // Inisialisasi sensor DHT22
  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Koneksi WiFi
  koneksiWiFi();

  // Sinkronisasi waktu WIB
  configTime(
    7 * 3600,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  Serial.println("# Menunggu sinkronisasi waktu...");

  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }

  // Membuat identitas perangkat
  clientId =
    "ESP32_S6_" +
    String((uint32_t)ESP.getEfuseMac(), HEX);

  // Membuat topik MQTT khusus S6
  topic = "josjis/s6/echo/" + clientId;

  // Pengaturan MQTT
  mqttClient.setId(clientId.c_str());
  mqttClient.setKeepAliveInterval(60000);
  mqttClient.setConnectionTimeout(5000);
  mqttClient.onMessage(onMqttMessage);

  Serial.println("# Identitas perangkat: " + clientId);
  Serial.println("# Topik MQTT: " + topic);

  // Koneksi broker MQTT
  if (!koneksiMQTT()) {
    Serial.println("# MQTT tidak berhasil terhubung.");
    Serial.println("# Pengujian dihentikan.");
    while (true) {
      delay(1000);
    }
  }

  // Metadata pengujian
  Serial.println();
  Serial.println("# SKENARIO S6");
  Serial.println("# Protokol: MQTT QoS 1");
  Serial.println("# Payload target: 500 byte");
  Serial.println("# Target pesan: 100");
  Serial.println("# Jeda pengiriman: 2 detik");
  Serial.println("# Timeout: 5 detik");
  Serial.println("# Waktu mulai WIB: " + waktuWIB());

  // Header CSV
  Serial.println(
    "skenario,seq,payload_byte,waktu_absolut_wib,"
    "t_kirim_ms,t_terima_ms,rtt_ms,status,rssi_dbm"
  );
}

// ===============================
// LOOP PENGUJIAN
// ===============================
void loop() {

  for (int seq = 1; seq <= TARGET_PESAN; seq++) {

    // Menjaga jeda minimal 2 detik antarpengiriman
    while (
      seq > 1 &&
      millis() - lastSend < INTERVAL_MS
    ) {
      mqttClient.poll();
      delay(1);
    }

    // Membaca sensor DHT22
    TempAndHumidity data = dht.getTempAndHumidity();

    float suhu = data.temperature;
    float kelembapan = data.humidity;

    // Memeriksa hasil pembacaan sensor
    if (isnan(suhu) || isnan(kelembapan)) {
      Serial.println("# Pembacaan DHT22 gagal.");
      delay(2000);
      continue;
    }

    // Membuat payload tepat 500 byte
    expectedPayload = buatPayload(
      seq,
      suhu,
      kelembapan
    );

    // Mengatur status penerimaan
    echoReceived = false;
    echoMatches = false;
    waitingEcho = true;

    // Mencatat waktu pengiriman
    tKirim = millis();
    lastSend = tKirim;

    String waktuKirim = waktuWIB();

    // Mengirim MQTT dengan QoS 1
    mqttClient.beginMessage(
      topic.c_str(),
      expectedPayload.length(),
      false,
      1
    );

    mqttClient.print(expectedPayload);
    mqttClient.endMessage();

    // Menunggu pesan echo diterima
    while (
      !echoReceived &&
      millis() - tKirim < TIMEOUT_MS
    ) {
      mqttClient.poll();
      delay(1);
    }

    waitingEcho = false;

    // Menghitung RTT dan menentukan status
    long rtt = -1;
    String status = "TIMEOUT";
    String waktuTerima = "";

    if (echoReceived && echoMatches) {
      rtt = tTerima - tKirim;
      status = "BERHASIL";
      waktuTerima = String(tTerima);
    } else if (echoReceived && !echoMatches) {
      status = "GAGAL";
      waktuTerima = String(tTerima);
    }

    // Membaca RSSI WiFi
    int rssi = WiFi.RSSI();

    // Menampilkan hasil dalam format CSV
    Serial.print("S6,");
    Serial.print(seq);
    Serial.print(",");
    Serial.print(expectedPayload.length());
    Serial.print(",");
    Serial.print(waktuKirim);
    Serial.print(",");
    Serial.print(tKirim);
    Serial.print(",");
    Serial.print(waktuTerima);
    Serial.print(",");
    Serial.print(rtt);
    Serial.print(",");
    Serial.print(status);
    Serial.print(",");
    Serial.println(rssi);

    // Memberi kesempatan MQTT memproses pesan
    mqttClient.poll();
  }

  // Menampilkan waktu selesai
  Serial.println();
  Serial.println("# Pengujian S6 selesai.");
  Serial.println("# Waktu selesai WIB: " + waktuWIB());

  // Menghentikan pengulangan agar tidak mengirim ulang
  while (true) {
    mqttClient.poll();
    delay(1000);
  }
}