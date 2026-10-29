#include <dummy.h>

#include <dummy.h>

#include <LiquidCrystal.h>

#include <dummy.h>

#include <dummy.h>

#include <MAX30105.h>
#include <heartRate.h>
#include <spo2_algorithm.h>

#include <MAX30105.h>
#include <heartRate.h>
#include <spo2_algorithm.h>

/**
 * ============================================================
 *  glucose_monitor.ino  —  FILE UTAMA
 * ============================================================
 *
 *  ALAT DETEKSI GULA DARAH NON-INVASIF
 *  Hardware : ESP32 DevKit + MAX30102 + LCD 16x2 I2C
 *  Metode   : PPG Signal Feature Extraction + Regresi Linear
 *
 *  CARA PAKAI:
 *  1. Upload kode ini ke ESP32
 *  2. Tunggu layar welcome muncul di LCD
 *  3. Tempelkan jari ke permukaan sensor MAX30102
 *     (bagian yang ada LED merah kecilnya)
 *  4. Tahan diam selama 5-6 detik sampai progress bar penuh
 *  5. Baca hasil di LCD
 *
 *  LIBRARY YANG HARUS DIINSTALL:
 *  - SparkFun MAX3010x Pulse and Proximity Sensor Library
 *  - LiquidCrystal I2C  (by Frank de Brabander)
 *  Keduanya tersedia di Library Manager Arduino IDE.
 *
 *  DISCLAIMER:
 *  Ini adalah estimasi berbasis PPG. Akurasi ±15-25 mg/dL
 *  setelah kalibrasi. Bukan pengganti glucometer medis.
 * ============================================================
 */

// ============================================================
//  INCLUDE LIBRARY & FILE PENDUKUNG
// ============================================================
#include <Wire.h>              // Library I2C bawaan Arduino
#include "MAX30105.h"          // Library sensor MAX30102/MAX30105
#include "config.h"            // File konfigurasi kita (angka-angka)
#include "lcd_display.h"       // File tampilan LCD kita

// ============================================================
//  OBJEK SENSOR
// ============================================================
MAX30105 particleSensor;

// ============================================================
//  BUFFER PENYIMPANAN DATA PPG
//  Menampung SAMPLE_SIZE titik data dari Red LED dan IR LED.
//  Ukuran buffer = 500 sampel × 2 channel × 4 byte = ~4KB RAM
//  ESP32 punya 520KB RAM, jadi aman.
// ============================================================
long redBuffer[SAMPLE_SIZE];
long irBuffer[SAMPLE_SIZE];

// ============================================================
//  VARIABEL HASIL PENGUKURAN
// ============================================================
float heartRate       = 0.0f;   // Detak jantung (bpm)
float spo2            = 0.0f;   // Saturasi oksigen (%)
float glucoseEstimate = 0.0f;   // Estimasi gula darah (mg/dL)

// Fitur PPG yang diekstrak
float acRed          = 0.0f;   // Komponen AC sinyal Red (pulsatil)
float dcRed          = 0.0f;   // Komponen DC sinyal Red (baseline)
float acIR           = 0.0f;   // Komponen AC sinyal IR (pulsatil)
float dcIR           = 0.0f;   // Komponen DC sinyal IR (baseline)
float ratioRtoIR     = 0.0f;   // Rasio R = (AC_red/DC_red)/(AC_IR/DC_IR)
float pulseAmplitude = 0.0f;   // Amplitudo pulsa dari IR channel

// ============================================================
//  VARIABEL PUSH BUTTON & STATE POWER
// ============================================================
bool buttonPressed   = false;  // Flag: tombol sudah ditekan?
bool deviceOn        = false;  // Flag: alat aktif (true) atau standby/off (false)

// ============================================================
//  SETUP — Dijalankan sekali saat ESP32 dinyalakan
// ============================================================
void setup() {
  // Inisialisasi Serial Monitor (untuk debugging)
  Serial.begin(115200);
  Serial.println("\n=== GlucoScan PPG Glucose Monitor ===");

  // Inisialisasi I2C dengan pin yang ditentukan di config.h
  // Kenapa Wire.begin(SDA, SCL)?
  // ESP32 berbeda dari Arduino Uno — pin I2C bisa diassign custom.
  // Kalau tidak disebutkan, ESP32 pakai default GPIO 21/22 juga,
  // tapi lebih baik eksplisit supaya tidak bingung.
  Wire.begin(PIN_SDA, PIN_SCL);

  // Inisialisasi pin Buzzer dan Push Button
  // INPUT_PULLUP: GPIO aktif LOW — tombol ditekan = LOW, lepas = HIGH
  // Ini umum dipakai supaya tidak perlu resistor eksternal.
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);  // Pastikan buzzer mati saat startup
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Inisialisasi LCD
  lcdInit();
  lcdShowWelcome();

  // Inisialisasi sensor MAX30102
  Serial.print("Menginisialisasi MAX30102...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    // Sensor tidak terdeteksi
    // Kemungkinan penyebab:
    // 1. Kabel sambungan longgar atau salah
    // 2. Sensor rusak
    // 3. Alamat I2C salah (MAX30102 selalu 0x57, tidak bisa diubah)
    Serial.println(" GAGAL!");
    Serial.println("Cek koneksi SDA/SCL dan power 3.3V ke sensor.");
    lcdShowError("Sensor tak resp.");
    while (true) { delay(1000); }  // Berhenti di sini, perlu reset manual
  }

  Serial.println(" OK");

  // Konfigurasi parameter sensor
  // Penjelasan setiap parameter ada di config.h
  particleSensor.setup(
    LED_BRIGHTNESS,   // Kecerahan LED (mA)
    SAMPLE_AVERAGE,   // Rerata sampel
    2,                // LED mode: 2 = Red + IR (diperlukan untuk rasio R)
    SAMPLE_RATE,      // Frekuensi sampling (Hz)
    PULSE_WIDTH,      // Lebar pulsa (us) — menentukan resolusi
    ADC_RANGE         // Range ADC
  );

  Serial.println("Sensor siap. Tempelkan jari ke MAX30102.\n");
}

// ============================================================
//  LOOP — Dijalankan berulang-ulang selamanya
//
//  LOGIKA PUSH BUTTON (ON/OFF TOGGLE):
//  - Tekan tombol saat alat OFF → alat menyala, mulai pengukuran
//  - Tekan tombol saat alat ON  → alat mati, kembali ke layar off
//  Tombol dicek di tiap fase sehingga alat bisa dimatikan kapan saja.
// ============================================================
void loop() {
  // ============================================================
  //  MODE OFF — Layar mati, tunggu tombol untuk menyalakan
  // ============================================================
  if (!deviceOn) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(0));  // Ikon hati
    lcd.print(" LUMIGLU");
    lcd.setCursor(0, 1);
    lcd.print("Tekan utk nyala");

    // Polling tombol — TIDAK ada kode lain yang jalan di sini
    // sehingga tombol selalu terbaca dengan benar
    while (digitalRead(PIN_BUTTON) == HIGH) {
      delay(10);
    }
    delay(50);  // Debounce
    if (digitalRead(PIN_BUTTON) == HIGH) return;  // Noise, ulangi
    while (digitalRead(PIN_BUTTON) == LOW) { delay(10); }  // Tunggu dilepas

    // Tombol dilepas → nyalakan alat
    deviceOn = true;
    Serial.println("[POWER] Alat dinyalakan.");
    digitalWrite(PIN_BUZZER, HIGH);
    delay(80);
    digitalWrite(PIN_BUZZER, LOW);
    return;  // ← KUNCI: kembali ke atas loop() dulu sebelum lanjut
  }

  // ============================================================
  //  MODE ON — Standby, tunggu tombol mulai ukur atau OFF
  // ============================================================

  // Tampilkan layar standby sekali, lalu segera cek tombol
  lcdShowStandby();

  // Cek apakah tombol ditekan
  if (digitalRead(PIN_BUTTON) != LOW) {
    // Belum ditekan, putar balik ke atas loop (animasi terus jalan)
    delay(10);
    return;
  }

  // Tombol terdeteksi LOW → ukur berapa lama ditekan
  delay(50);  // Debounce
  if (digitalRead(PIN_BUTTON) == HIGH) return;  // Noise, abaikan

  unsigned long pressStart = millis();
  while (digitalRead(PIN_BUTTON) == LOW) { delay(10); }  // Tunggu dilepas
  unsigned long pressDuration = millis() - pressStart;

  if (pressDuration >= 800) {
    // ── Tombol panjang (≥800ms) → MATIKAN ALAT ──
    deviceOn = false;
    Serial.println("[POWER] Alat dimatikan.");
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_BUZZER, HIGH);
      delay(80);
      digitalWrite(PIN_BUZZER, LOW);
      delay(100);
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Alat mati...");
    delay(1000);
    return;  // Kembali ke atas loop → masuk MODE OFF
  }

  // ── Tombol pendek → MULAI PENGUKURAN ──
  Serial.println("[TOMBOL] Pengukuran dimulai!");
  digitalWrite(PIN_BUZZER, HIGH);
  delay(80);
  digitalWrite(PIN_BUZZER, LOW);

  // --- Langkah 1: Tunggu jari ditempelkan ---
  if (!waitForFinger()) {
    // Timeout — tidak ada jari selama FINGER_TIMEOUT_MS
    return;  // Ulangi dari awal loop
  }

  // --- Langkah 2: Kumpulkan data PPG dari sensor (5 detik) ---
  collectSamples();

  // --- Langkah 3: Tampilkan "memproses" di LCD ---
  lcdShowProcessing();

  // --- Langkah 4: Ekstrak fitur dari sinyal PPG ---
  extractPPGFeatures();

  // --- Langkah 5: Hitung estimasi gula darah ---
  calculateGlucose();

  // --- Langkah 6: Hitung SpO2 (bonus) ---
  calculateSpO2();

  // --- Langkah 7: Tampilkan hasil + buzzer bersamaan ---
  // Buzzer sekarang dipanggil di DALAM displayResults(),
  // tepat saat angka gula darah pertama kali muncul di LCD.
  displayResults();

  // --- Langkah 8: Tunggu sebelum kembali ke standby ---
  lcdShowReady();
  // Cek tombol selama 5 detik, kalau ditekan langsung lanjut
  unsigned long waitStart = millis();
  while (millis() - waitStart < 5000) {
    if (digitalRead(PIN_BUTTON) == LOW) {
      delay(50);
      if (digitalRead(PIN_BUTTON) == LOW) {
        while (digitalRead(PIN_BUTTON) == LOW) { delay(10); }
        break;  // Langsung lanjut tanpa tunggu penuh
      }
    }
    delay(10);
  }
}

// ============================================================
//  FUNGSI: Tunggu Push Button Ditekan
//
//  Alat masuk mode standby setelah tiap pengukuran.
//  Pengukuran baru hanya dimulai setelah tombol ditekan.
//  Debounce sederhana: tunggu 50ms setelah deteksi LOW
//  untuk memastikan bukan noise/bouncing kontak tombol.
// ============================================================
void waitForButton() {
  Serial.println("Menunggu tombol ditekan...");
  buttonPressed = false;

  while (!buttonPressed) {
    if (digitalRead(PIN_BUTTON) == LOW) {
      delay(50);  // Debounce
      if (digitalRead(PIN_BUTTON) == LOW) {
        buttonPressed = true;
        Serial.println("[TOMBOL] Pengukuran dimulai!");
        // Konfirmasi bunyi pendek 1x saat tombol ditekan
        digitalWrite(PIN_BUZZER, HIGH);
        delay(80);
        digitalWrite(PIN_BUZZER, LOW);
      }
    }
    delay(10);  // Polling setiap 10ms
  }
}

// ============================================================
//  FUNGSI: Buzzer Alert Berdasarkan Hasil Gula
//
//  Pola bunyi:
//    RENDAH (hipoglikemia) → 2 kali bunyi
//      Sinyal bahaya: gula terlalu rendah, perlu tindakan segera
//    NORMAL                → 1 kali bunyi
//      Konfirmasi: hasil baik
//    BATAS TINGGI / TINGGI → 3 kali bunyi
//      Peringatan: gula tinggi, perlu perhatian
//
//  Parameter:
//    glucose = nilai estimasi dalam mg/dL
// ============================================================
void buzzerAlert(float glucose) {
  int beepCount;

  if (glucose < GLUCOSE_HIPO) {
    beepCount = 2;  // Rendah: 2x bunyi
  } else if (glucose < GLUCOSE_NORMAL) {
    beepCount = 1;  // Normal: 1x bunyi
  } else {
    beepCount = 3;  // Tinggi / Batas Tinggi: 3x bunyi
  }

  Serial.printf("[BUZZER] %d kali bunyi untuk kategori glukosa %.1f mg/dL\n",
                beepCount, glucose);

  for (int i = 0; i < beepCount; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(BUZZER_BEEP_MS);
    digitalWrite(PIN_BUZZER, LOW);
    if (i < beepCount - 1) {
      delay(BUZZER_PAUSE_MS);  // Jeda antar bunyi (kecuali bunyi terakhir)
    }
  }
}

// ============================================================
//  FUNGSI: Tunggu Jari Ditempelkan
//
//  Cara deteksi jari:
//  Sensor IR memancarkan cahaya. Kalau tidak ada jari,
//  cahaya banyak terpencar ke udara → nilai ADC rendah.
//  Kalau ada jari, cahaya dipantulkan kembali → nilai ADC tinggi.
//  Threshold 50000 (dari range 0-262143 pada ADC 18-bit)
//  menandakan ada massa yang memantulkan cahaya di atas sensor.
//
//  Return: true kalau jari terdeteksi, false kalau timeout
// ============================================================
bool waitForFinger() {
  unsigned long startTime = millis();
  Serial.print("Menunggu jari");

  while (millis() - startTime < FINGER_TIMEOUT_MS) {
    // Animasi di LCD
    lcdShowWaitFinger();

    // Cek nilai IR dari sensor
    long irValue = particleSensor.getIR();

    if (irValue > FINGER_THRESHOLD) {
      // Jari terdeteksi!
      Serial.println(" [TERDETEKSI]");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Jari terdeteksi!");
      lcd.setCursor(0, 1);
      lcd.print("Tahan diam...");
      delay(1000);
      return true;
    }

    // Print titik di Serial setiap 500ms
    if ((millis() - startTime) % 500 < 50) {
      Serial.print(".");
    }
  }

  // Timeout
  Serial.println("\n[TIMEOUT] Tidak ada jari terdeteksi.");
  lcdShowError("Timeout! Coba lg");
  delay(2000);
  return false;
}

// ============================================================
//  FUNGSI: Kumpulkan Sampel PPG
//
//  Mengambil SAMPLE_SIZE titik data dari kedua channel.
//  Pada 100 Hz, 500 sampel = 5 detik data.
//  5 detik cukup untuk menangkap sekitar 5-7 siklus detak
//  jantung (asumsi HR 60-80 bpm).
// ============================================================
void collectSamples() {
  Serial.print("Mengambil data PPG");

  for (int i = 0; i < SAMPLE_SIZE; i++) {
    // Tunggu sampai data baru tersedia di buffer sensor
    while (!particleSensor.available()) {
      particleSensor.check();  // Polling sensor
    }

    // Ambil data dari buffer sensor
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i]  = particleSensor.getIR();
    particleSensor.nextSample();  // Maju ke sampel berikutnya

    // Update progress bar di LCD setiap 25 sampel
    if (i % 25 == 0) {
      int progress = (i * 100) / SAMPLE_SIZE;
      lcdShowProgress(progress);
    }

    // Progress di Serial setiap 100 sampel
    if (i % 100 == 0) Serial.print(".");
  }

  // Progress 100% sebelum lanjut
  lcdShowProgress(100);
  delay(300);
  Serial.println(" [SELESAI]");
}

// ============================================================
//  FUNGSI: Ekstraksi Fitur PPG
//
//  Mengubah array 500 titik data mentah menjadi 4 angka
//  yang merangkum karakteristik sinyal PPG.
//
//  ── Fitur 1 & 2: DC Component (rata-rata) ──
//  DC = mean dari seluruh sampel.
//  Merepresentasikan:
//  - Cahaya yang diserap oleh jaringan (kulit, lemak, tulang)
//  - Cahaya yang diserap darah vena (tidak berdenyut)
//  - Nilai baseline yang tidak berubah per detak
//
//  ── Fitur 3 & 4: AC Component (amplitudo puncak-lembah) ──
//  AC = nilai maksimum minus nilai minimum dalam window tengah.
//  Kenapa window tengah (25%-75% buffer)?
//  Bagian awal dan akhir pengukuran sering ada artefak
//  karena jari baru ditempelkan/mau diangkat. Window tengah
//  paling stabil.
//  Merepresentasikan:
//  - Perubahan volume darah arteri per detak jantung
//  - Komponen "pulsatil" sinyal
//
//  ── Fitur 5: Rasio R ──
//  R = (AC_red/DC_red) / (AC_IR/DC_IR)
//  Ini adalah rasio yang dinormalisasi — membagi AC dengan DC
//  supaya hasilnya tidak tergantung intensitas cahaya absolute
//  (yang bisa beda tergantung tebal jari, warna kulit, dll).
//  Rasio R ini aslinya digunakan untuk menghitung SpO2,
//  tapi juga berkorelasi dengan kadar glukosa karena
//  glukosa mempengaruhi sifat optik darah secara berbeda
//  di wavelength Red vs IR.
//
//  ── Fitur 6: Heart Rate ──
//  Dihitung dari jumlah zero-crossing pada sinyal IR.
// ============================================================
void extractPPGFeatures() {
  Serial.println("Mengekstrak fitur PPG...");

  // --- DC Component: rata-rata semua sampel ---
  long sumRed = 0, sumIR = 0;
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    sumRed += redBuffer[i];
    sumIR  += irBuffer[i];
  }
  dcRed = (float)sumRed / SAMPLE_SIZE;
  dcIR  = (float)sumIR  / SAMPLE_SIZE;

  // --- AC Component: puncak-ke-lembah pada window tengah ---
  // Window: sampel ke-125 sampai ke-375 (50% tengah buffer)
  int windowStart = SAMPLE_SIZE / 4;      // = 125
  int windowEnd   = 3 * SAMPLE_SIZE / 4;  // = 375

  long maxRed = redBuffer[windowStart];
  long minRed = redBuffer[windowStart];
  long maxIR  = irBuffer[windowStart];
  long minIR  = irBuffer[windowStart];

  for (int i = windowStart; i < windowEnd; i++) {
    if (redBuffer[i] > maxRed) maxRed = redBuffer[i];
    if (redBuffer[i] < minRed) minRed = redBuffer[i];
    if (irBuffer[i]  > maxIR)  maxIR  = irBuffer[i];
    if (irBuffer[i]  < minIR)  minIR  = irBuffer[i];
  }

  acRed = (float)(maxRed - minRed);
  acIR  = (float)(maxIR  - minIR);
  pulseAmplitude = acIR;

  // --- Rasio R ---
  // Guard: pastikan tidak bagi nol
  if (dcRed > 0 && dcIR > 0 && acIR > 100) {
    ratioRtoIR = (acRed / dcRed) / (acIR / dcIR);
  } else {
    ratioRtoIR = 0.8f;  // Default kalau sinyal terlalu lemah
  }

  // --- Heart Rate ---
  heartRate = calculateHeartRate();

  // Log ke Serial untuk debugging
  Serial.printf("  DC Red  : %.0f | DC IR  : %.0f\n", dcRed, dcIR);
  Serial.printf("  AC Red  : %.0f | AC IR  : %.0f\n", acRed, acIR);
  Serial.printf("  Rasio R : %.4f\n", ratioRtoIR);
  Serial.printf("  Amp     : %.0f\n", pulseAmplitude);
  Serial.printf("  HR      : %.1f bpm\n", heartRate);
}

// ============================================================
//  FUNGSI: Hitung Heart Rate dari Sinyal IR
//
//  Algoritma zero-crossing:
//  1. Hitung nilai rata-rata (DC) sebagai threshold
//  2. Hitung berapa kali sinyal menyeberangi threshold
//     dari bawah ke atas (rising edge)
//  3. Setiap rising edge = 1 detak jantung
//  4. HR = (jumlah detak / durasi rekaman) × 60
//
//  Kenapa pakai zero-crossing dan bukan peak detection?
//  Zero-crossing lebih tahan terhadap noise.
//  Peak detection butuh threshold dinamis yang lebih kompleks.
//  Untuk aplikasi ini, zero-crossing sudah cukup akurat.
// ============================================================
float calculateHeartRate() {
  // Hitung rata-rata sebagai baseline threshold
  long dcVal = 0;
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    dcVal += irBuffer[i];
  }
  dcVal /= SAMPLE_SIZE;

  // Hitung rising edge crossing
  int crossings = 0;
  bool wasBelow = (irBuffer[0] < dcVal);

  for (int i = 1; i < SAMPLE_SIZE; i++) {
    bool isBelow = (irBuffer[i] < dcVal);
    // Rising edge: sebelumnya di bawah, sekarang di atas
    if (wasBelow && !isBelow) {
      crossings++;
    }
    wasBelow = isBelow;
  }

  // Hitung HR: crossings = jumlah detak dalam SAMPLE_SIZE/SAMPLE_RATE detik
  float durationSec = (float)SAMPLE_SIZE / SAMPLE_RATE;
  float hr = ((float)crossings / durationSec) * 60.0f;

  // Clamp ke range fisiologis manusia (40-200 bpm)
  // Di luar range ini kemungkinan noise atau artefak gerakan
  if (hr < 40.0f || hr > 200.0f) {
    Serial.println("  [WARN] HR di luar range, pakai default 75 bpm");
    hr = 75.0f;
  }

  return hr;
}

// ============================================================
//  FUNGSI: Hitung Estimasi Gula Darah
//
//  Model: Regresi Linear 2 Prediktor (dari data kalibrasi 18 sampel)
//
//  Formula:
//  Glucose = COEF_R * ratioRtoIR + COEF_IR * dcIR + COEF_INTERCEPT
//
//  Koefisien (lihat detail penjelasan di config.h):
//    COEF_R         = -178.2053  (rasio R/IR dominan, hubungan terbalik)
//    COEF_IR        = -0.0011237 (koreksi perfusi antar individu)
//    COEF_INTERCEPT = 390.1194   (baseline/offset kalibrasi)
//
//  Performa model (dari 18 data kalibrasi real):
//    R²   = 0.9346  (sangat baik, target ≥ 0.85)
//    RMSE = 7.55 mg/dL
//    MAPE = 6.03%
//
//  CARA REKALIBRASI PERSONAL (jika hasil meleset konsisten):
//    Hasil alat selalu terlalu tinggi → turunkan COEF_INTERCEPT di config.h
//    Hasil alat selalu terlalu rendah  → naikkan COEF_INTERCEPT di config.h
// ============================================================
void calculateGlucose() {
  glucoseEstimate = COEF_R          * ratioRtoIR
                  + COEF_IR         * dcIR
                  + COEF_INTERCEPT;

  // Clamp ke range fisiologis yang masuk akal
  glucoseEstimate = constrain(glucoseEstimate,
                              GLUCOSE_MIN_CLAMP,
                              GLUCOSE_MAX_CLAMP);
}

// ============================================================
//  FUNGSI: Hitung SpO2 (Saturasi Oksigen)
//
//  SpO2 = 110 - 25 × R
//
//  Ini adalah rumus empiris standar yang digunakan di hampir
//  semua pulse oximeter. Rasio R berada di sekitar:
//  R ≈ 0.5 → SpO2 ≈ 97.5% (normal)
//  R ≈ 1.0 → SpO2 ≈ 85% (hipoksemia)
//
//  Rumus ini divalidasi oleh Moyle (2002) dan merupakan
//  standar industri pulse oximetry.
// ============================================================
void calculateSpO2() {
  if (ratioRtoIR > 0.4f && ratioRtoIR < 1.5f) {
    spo2 = 110.0f - 25.0f * ratioRtoIR;
    spo2 = constrain(spo2, 70.0f, 100.0f);
  } else {
    spo2 = 0.0f;  // Tidak valid
  }
}

// ============================================================
//  FUNGSI: Tampilkan Semua Hasil
//  Mengorkestrasi urutan tampilan di LCD dan output Serial
// ============================================================
void displayResults() {
  // Tentukan kategori dan ikon berdasarkan nilai glukosa
  const char* category;
  byte icon;
  String categoryStr;

  if (glucoseEstimate < GLUCOSE_HIPO) {
    category = "HIPO <70";
    icon = 5;  // Ikon panah turun
  }
  else if (glucoseEstimate < GLUCOSE_NORMAL) {
    category = "NORMAL";
    icon = 3;  // Ikon ceklis
  }
  else if (glucoseEstimate < GLUCOSE_PREDIABET) {
    category = "PRADIABET";
    icon = 4;  // Ikon panah naik
  }
  else {
    category = "DIABETES";
    icon = 4;  // Ikon panah naik
  }

  // ── Tampilkan di LCD ──

  // Hasil utama: angka gula darah — buzzer berbunyi bersamaan saat ini
  lcdShowGlucoseResult(glucoseEstimate, category, icon);
  buzzerAlert(glucoseEstimate);  // Bunyi tepat saat angka muncul di LCD
  delay(DISPLAY_DELAY_MS);

  // Detail fitur PPG
  lcdShowDetails(heartRate, spo2, ratioRtoIR, pulseAmplitude);

  // Saran tindakan
  lcdShowAdvice(glucoseEstimate);

  // ── Output ke Serial Monitor ──
  // Berguna untuk logging dan debugging

  Serial.println("\n========================================");
  Serial.println("        HASIL PENGUKURAN");
  Serial.println("========================================");
  Serial.printf("  Heart Rate    : %.1f bpm\n", heartRate);
  Serial.printf("  SpO2          : %.1f %%\n", spo2);
  Serial.println("----------------------------------------");
  Serial.printf("  DC Red/IR     : %.0f / %.0f\n", dcRed, dcIR);
  Serial.printf("  AC Red/IR     : %.0f / %.0f\n", acRed, acIR);
  Serial.printf("  Rasio R       : %.4f\n", ratioRtoIR);
  Serial.printf("  Amplitudo     : %.0f ADC\n", pulseAmplitude);
  Serial.println("----------------------------------------");
  Serial.printf("  ESTIMASI GDS  : %.1f mg/dL\n", glucoseEstimate);
  Serial.printf("  KATEGORI GDS  : %s\n", category);
  Serial.println("  Acuan: Hipo<70 | Normal 70-139 | Pradiabet 140-199 | Diabetes>=200");
  Serial.println("========================================");

  // Output CSV untuk plotting/logging eksternal
  // Format: waktu_ms, glukosa, HR, SpO2, rasioR, AC_IR
  Serial.printf("[CSV] %lu,%.1f,%.1f,%.1f,%.4f,%.0f\n",
    millis(), glucoseEstimate, heartRate, spo2, ratioRtoIR, acIR);
}
