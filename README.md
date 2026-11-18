# Pengujian ESP32 – Bagian A dan Bagian B

Repositori ini berisi kode ESP32, data pengujian, serta file Excel untuk pengolahan dan analisis hasil pengujian Bagian A dan Bagian B.

## Struktur repositori

```text
Pengujian-ESP32/
├── README.md
├── .gitignore
├── Bagian-A/
│   ├── PembacaanSuhu.ino
│   └── Analisis Pemantauan 7 Hari.xlsx
└── Bagian-B/
    ├── Kode/
    │   ├── Skenario1.ino
    │   ├── Skenario2.ino
    │   ├── Skenario3.ino
    │   ├── Skenario4.ino
    │   ├── Skenario5.ino
    │   └── Skenario6.ino
    └── Data_Analisis/
        ├── Enam Skenario (Wokwi).xlsx
        └── S1-S6_overhead (Wireshark).xlsx
```

## Isi dan metode analisis

### Bagian A

- `PembacaanSuhu.ino`: program ESP32 untuk membaca suhu dan kelembapan menggunakan DHT11 dan mengirimkan data ke ThingSpeak.
- `Analisis Pemantauan 7 Hari.xlsx`: berisi sheet `Petunjuk`, `Data Pengujian`, `Ringkasan Harian`, dan `Ringkasan Keseluruhan`. Perhitungan dilakukan menggunakan rumus Microsoft Excel.

### Bagian B

Enam program pada folder `Kode` mewakili skenario pengujian HTTP dan MQTT:

- S1: HTTP POST, payload 20 byte.
- S2: HTTP POST, payload 500 byte.
- S3: MQTT QoS 0, payload 20 byte.
- S4: MQTT QoS 0, payload 500 byte.
- S5: MQTT QoS 1, payload 20 byte.
- S6: MQTT QoS 1, payload 500 byte.

`Enam Skenario (Wokwi).xlsx` berisi sheet S1 sampai S6 dan `Perhitungan`. Data hasil pengujian Wokwi dan perhitungan parameter seperti RTT dan PDR diolah menggunakan rumus Excel.

`S1-S6_overhead (Wireshark).xlsx` berisi sheet `S1_packets` sampai `S6_packets` serta `Perhitungan`. Data paket diambil melalui Wireshark, kemudian disimpan pada sheet masing-masing skenario dan digunakan untuk menghitung overhead menggunakan rumus Excel.

**Catatan:** File Excel dipertahankan dalam bentuk workbook dengan seluruh sheet dan rumusnya. Tidak ada skrip Python terpisah karena pengolahan data dilakukan di Microsoft Excel.

## Dependensi dan perangkat lunak

Pengujian menggunakan perangkat lunak dan pustaka berikut:

| Perangkat lunak/pustaka | Versi/keterangan | Penggunaan |
|---|---|---|
| Arduino IDE | 2.3.10 | Penulisan dan kompilasi program Bagian A |
| ESP32 Arduino Core | 2.0.18-arduino.5 | Dukungan board ESP32 |
| Adafruit DHT Sensor Library | 1.4.7 | Pembacaan sensor DHT11 pada Bagian A |
| DHTesp | Versi tidak dicatat | Pembacaan sensor DHT22 pada Bagian B |
| PubSubClient | Versi tidak dicatat | Komunikasi MQTT pada Bagian B |
| ArduinoMqttClient | Versi tidak dicatat | Komunikasi MQTT pada Bagian B |
| Wokwi | Simulator online | Simulasi rangkaian dan pengujian Bagian B |
| Wireshark | 4.6.9 | Pengambilan dan analisis paket jaringan |
| Microsoft Excel | Versi 2610 | Pengolahan data dan perhitungan hasil pengujian |

**Catatan:**
- Pustaka `WiFi.h`, `HTTPClient.h`, dan `time.h` digunakan melalui lingkungan ESP32 Arduino Core.
- Versi `DHTesp`, `PubSubClient`, dan `ArduinoMqttClient` tidak dicatat secara pasti. Pada Bagian B, pustaka digunakan melalui lingkungan Wokwi sehingga versi yang digunakan perlu disesuaikan dengan konfigurasi pustaka pada proyek Wokwi.
- Pastikan seluruh pustaka yang diperlukan tersedia sebelum menjalankan program.

## Konfigurasi aman

- Jangan menyimpan kata sandi Wi-Fi, API Key ThingSpeak, token, atau kredensial pribadi ke repositori.
- Pada Bagian A, isi `WIFI_SSID`, `WIFI_PASS`, dan `TS_WRITE_KEY` secara lokal sebelum menjalankan program. Gunakan kredensial milik sendiri.
- Pada Bagian B, sketch menggunakan jaringan simulasi `Wokwi-GUEST` dan layanan uji publik yang tercantum di dalam kode. Jangan mengirim data sensitif melalui layanan uji publik.
- Jika kredensial pernah terunggah ke repositori publik, segera ganti atau cabut kredensial tersebut.

## Langkah menjalankan

### Bagian A – ESP32 dan DHT11

1. Buka `Bagian-A/PembacaanSuhu.ino` di Arduino IDE.
2. Pasang ESP32 Arduino Core dan library DHT yang tercantum pada bagian dependensi.
3. Hubungkan DHT11 DATA ke GPIO 4, VCC ke 3.3V, dan GND ke GND ESP32 sesuai rangkaian pengujian.
4. Isi konfigurasi Wi-Fi dan Write API Key ThingSpeak secara lokal.
5. Pilih board dan port ESP32 yang sesuai, lalu kompilasi dan unggah program.
6. Periksa Serial Monitor dan channel ThingSpeak untuk memastikan data diterima.
7. Unduh hasil pembacaan sensor dari ThingSpeak dalam format CSV, kemudian masukkan data ke sheet sumber data pada `Bagian-A/Analisis Pemantauan 7 Hari.xlsx`.
8. Sesuaikanlah format jika masih berantakan. Gunakan Excel dengan kalkulasi otomatis untuk menghitung ulang rumus.

### Bagian B – Enam skenario Wokwi

1. Buka sketch S1–S6 yang sesuai di folder `Bagian-B/Kode` atau salin kode ke proyek ESP32 pada Wokwi.
2. Siapkan rangkaian ESP32 dan DHT22 pada GPIO 15, dengan VCC ke 3.3 V dan GND ke GND, sesuai konfigurasi sketch.
3. Pastikan dependensi library untuk skenario yang dipilih tersedia pada proyek Wokwi.
4. Jalankan setiap skenario secara terpisah dan kumpulkan keluaran Serial Monitor sesuai prosedur pengujian.
5. Masukkan atau cocokkan data hasil pengujian pada workbook `Enam Skenario (Wokwi).xlsx` dan periksa sheet `Perhitungan`.
6. Untuk menghitung overhead, ambil paket dari setiap skenario menggunakan Wireshark, ekspor hasil penyaringan dalam format CSV, lalu masukkan data ke sheet S1–S6 yang sesuai pada `S1-S6_overhead (Wireshark).xlsx`.
7. Periksa sheet `Perhitungan` pada workbook overhead dan pastikan rumus Excel menghitung ulang hasilnya.

## Menghitung ulang hasil analisis Excel

1. Buka workbook yang ingin diperiksa menggunakan Microsoft Excel.
2. Pastikan seluruh sheet sumber data dan `Perhitungan` tetap tersedia.
3. Pilih **Formulas → Calculation Options → Automatic** agar rumus dihitung secara otomatis.
4. Gunakan **Calculate Now** atau tekan `Ctrl+Alt+F9` jika ingin menghitung ulang seluruh rumus.
5. Periksa hasil perhitungan pada sheet `Ringkasan Harian`, `Ringkasan Keseluruhan`, atau `Perhitungan` sesuai workbook yang dibuka.
6. Periksa grafik pada workbook Bagian A untuk memastikan tampilannya sesuai dengan data.

Catatan penting: Rumus pada workbook menggunakan rentang sel tertentu. Jika menambahkan data baru melebihi rentang yang sudah digunakan rumus, rentang rumus juga perlu disesuaikan. Oleh karena itu, jangan hanya menempelkan data tambahan tanpa memeriksa kembali rumusnya.

## Hasil verifikasi

Status verifikasi perlu diisi setelah setiap sketch berhasil dikompilasi/dijalankan ulang dan hasil perhitungan Excel dibandingkan dengan hasil pengujian. Jangan menyatakan pengujian ulang berhasil sebelum proses tersebut dilakukan.

- Kompilasi dan pengujian Bagian A: belum diverifikasi ulang saat penyusunan repositori.
- Pengujian enam skenario Bagian B: belum diverifikasi ulang saat penyusunan repositori.
- Perhitungan ulang workbook Excel: belum diverifikasi ulang saat penyusunan repositori.

## Versi dan commit

- Versi rilis: belum ditetapkan.
- Commit: akan tercatat otomatis saat commit pertama dibuat di GitHub.
- Tanggal penyusunan paket: 18 November 2026.
