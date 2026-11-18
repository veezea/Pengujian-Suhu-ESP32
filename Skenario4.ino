#include <WiFi.h>
#include <PubSubClient.h>
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
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

String clientId;
String topic;
String expectedPayload;

// ===============================
// KONFIGURASI PENGUJIAN
// ===============================
const int JUMLAH_PESAN = 100;
const int TARGET_PAYLOAD = 500;

const unsigned long INTERVAL = 2000;
const unsigned long TIMEOUT = 5000;

int berhasil = 0;
int gagal = 0;

bool echoDiterima = false;
unsigned long waktuTerima = 0;

String waktuMulaiAbsolut;

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
// MEMBUAT PAYLOAD 500 BYTE
// ===============================
String buatPayload(int seq, float suhu, float kelembapan) {

  String payload =
    "S4," + String(seq) +
    "," + String(suhu, 1) +
    "," + String(kelembapan, 1) +
    ",data=";

  while (payload.length() < TARGET_PAYLOAD) {
    payload += "A";
  }

  return payload;
}

// ===============================
// CALLBACK MQTT
// ===============================
void callback(
  char* receivedTopic,
  byte* payload,
  unsigned int length
) {

  if (String(receivedTopic) != topic) {
    return;
  }

  if (length != expectedPayload.length()) {
    return;
  }

  bool sama = true;

  for (unsigned int i = 0; i < length; i++) {
    if ((char)payload[i] != expectedPayload[i]) {
      sama = false;
      break;
    }
  }

  if (sama && !echoDiterima) {
    waktuTerima = millis();
    echoDiterima = true;
  }
}

// ===============================
// KONEKSI MQTT
// ===============================
bool koneksiMQTT() {

  int percobaan = 0;

  while (!client.connected() && percobaan < 5) {

    Serial.println("Menghubungkan ke broker MQTT...");

    if (client.connect(clientId.c_str())) {

      Serial.println("MQTT berhasil terhubung.");

      if (client.subscribe(topic.c_str(), 0)) {
        Serial.println("Subscribe berhasil.");
        return true;
      } else {
        Serial.println("Subscribe gagal.");
      }

    } else {

      Serial.print("Koneksi MQTT gagal. Kode: ");
      Serial.println(client.state());

    }

    percobaan++;
    delay(2000);
  }

  return client.connected();
}

// ===============================
// SETUP
// ===============================
void setup() {

  Serial.begin(115200);

  dht.setup(DHT_PIN, DHTesp::DHT22);

  // Menghubungkan Wi-Fi
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

  // Mengatur waktu WIB
  configTime(7 * 3600, 0, "pool.ntp.org");

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 15000)) {
    Serial.println("Gagal mendapatkan waktu NTP.");
    while (true) {
      delay(1000);
    }
  }

  // Membuat ID dan topik unik
  clientId = "ESP32_S4_" +
    String((uint32_t)ESP.getEfuseMac(), HEX);

  topic = "josjis/s4/echo/" + clientId;

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  // Buffer harus lebih besar dari payload 500 byte
  if (!client.setBufferSize(700)) {
    Serial.println("Gagal mengatur buffer MQTT.");
    while (true) {
      delay(1000);
    }
  }

  client.setKeepAlive(60);
  client.setSocketTimeout(5);

  Serial.println("\nMenghubungkan MQTT...");

  if (!koneksiMQTT()) {
    Serial.println("MQTT tidak berhasil terhubung.");
    Serial.println("Periksa broker atau koneksi internet.");
    while (true) {
      delay(1000);
    }
  }

  // Metadata pengujian
  waktuMulaiAbsolut = waktuWIB();

  Serial.println("\n=== METADATA S4 ===");
  Serial.println("Skenario: S4");
  Serial.println("Protokol: MQTT QoS 0");
  Serial.println("Target payload: 500 byte");
  Serial.println("Target pesan: 100");
  Serial.println("Interval: 2 detik");
  Serial.println("Timeout: 5000 ms");
  Serial.println("Perangkat: ESP32");
  Serial.println("Sensor: DHT22");
  Serial.println("Waktu mulai WIB: " + waktuMulaiAbsolut);
  Serial.println("Broker: broker.emqx.io");
  Serial.println("Port: 1883");

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

  unsigned long waktuMulai = millis();
  unsigned long jadwalKirim = waktuMulai;

  for (int seq = 1; seq <= JUMLAH_PESAN; seq++) {

    // Menjaga jarak minimal 2 detik
    while ((long)(millis() - jadwalKirim) < 0) {
      client.loop();
      delay(1);
    }

    // Memastikan MQTT masih terhubung
    if (!client.connected()) {

      Serial.println("Koneksi MQTT terputus.");

      if (!koneksiMQTT()) {
        Serial.println("Pengujian dihentikan karena MQTT terputus.");
        gagal += (JUMLAH_PESAN - seq + 1);
        break;
      }
    }

    client.loop();

    // Membaca sensor
    TempAndHumidity data = dht.getTempAndHumidity();

    if (isnan(data.temperature) || isnan(data.humidity)) {

      Serial.println(
        "S4," + String(seq) +
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

    // Memastikan ukuran payload tepat 500 byte
    if (payloadByte != TARGET_PAYLOAD) {

      Serial.println(
        "S4," + String(seq) +
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

    // Mengirim pesan MQTT QoS 0
    bool terkirim = client.publish(
      topic.c_str(),
      expectedPayload.c_str(),
      false
    );

    if (!terkirim) {

      Serial.println(
        "S4," + String(seq) + "," +
        String(payloadByte) + "," + timestamp +
        "," + String(tKirim) +
        ",0,0,GAGAL_PUBLISH," +
        String(WiFi.RSSI())
      );

      gagal++;

    } else {

      // Menunggu pesan echo maksimal 5 detik
      unsigned long batasWaktu = millis();

      while (
        !echoDiterima &&
        millis() - batasWaktu < TIMEOUT
      ) {
        client.loop();
        delay(1);
      }

      if (echoDiterima) {

        unsigned long rtt = waktuTerima - tKirim;

        Serial.println(
          "S4," + String(seq) + "," +
          String(payloadByte) + "," + timestamp +
          "," + String(tKirim) + "," +
          String(waktuTerima) + "," +
          String(rtt) + ",BERHASIL," +
          String(WiFi.RSSI())
        );

        berhasil++;

      } else {

        Serial.println(
          "S4," + String(seq) + "," +
          String(payloadByte) + "," + timestamp +
          "," + String(tKirim) +
          ",0,0,TIMEOUT," +
          String(WiFi.RSSI())
        );

        gagal++;
      }
    }

    jadwalKirim += INTERVAL;
  }

  // ===============================
  // HASIL PENGUJIAN
  // ===============================
  Serial.println("\n=== HASIL PENGUJIAN S4 ===");

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