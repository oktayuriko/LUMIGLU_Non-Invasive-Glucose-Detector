#include <dummy.h>

#include <dummy.h>

#include <LiquidCrystal_I2C.h>

#include <LiquidCrystal_I2C.h>

/**
 * ============================================================
 *  lcd_display.h  —  FUNGSI TAMPILAN LCD
 * ============================================================
 *  Berisi semua fungsi yang mengatur apa yang tampil di LCD.
 *  Dipisah dari file utama supaya kode lebih rapi dan mudah
 *  diubah tampilan tanpa ganggu logika pengukuran.
 *
 *  LIBRARY YANG DIBUTUHKAN:
 *  Install via Library Manager: "LiquidCrystal I2C" by Frank de Brabander
 * ============================================================
 */

#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <LiquidCrystal_I2C.h>
#include "config.h"

// ============================================================
//  OBJEK LCD
//  Parameter: (alamat I2C, jumlah kolom, jumlah baris)
// ============================================================
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// ============================================================
//  KARAKTER KUSTOM UNTUK LCD
//  LCD 16x2 bisa menyimpan 8 karakter kustom (index 0-7).
//  Setiap karakter adalah grid 5x8 pixel, ditulis per baris
//  sebagai biner (1=titik hitam, 0=putih).
//
//  Kenapa perlu karakter kustom?
//  LCD standar tidak punya simbol hati, batang progress,
//  atau ikon medis. Kita buat sendiri dengan bitmap 5x8.
// ============================================================

// Karakter 0: ikon hati (untuk layar selamat datang)
byte charHeart[8] = {
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000,
  0b00000
};

// Karakter 1: batang penuh (untuk progress bar loading)
byte charBarFull[8] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};

// Karakter 2: batang kosong (untuk progress bar loading)
byte charBarEmpty[8] = {
  0b11111,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b11111
};

// Karakter 3: ikon ceklis (untuk hasil normal)
byte charCheck[8] = {
  0b00000,
  0b00001,
  0b00011,
  0b10110,
  0b11100,
  0b01000,
  0b00000,
  0b00000
};

// Karakter 4: ikon panah naik (untuk hasil tinggi)
byte charUp[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};

// Karakter 5: ikon panah turun (untuk hasil rendah)
byte charDown[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

// Karakter 6: ikon titik berkedip (animasi mengukur)
byte charDot[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b01110,
  0b01110,
  0b00000,
  0b00000,
  0b00000
};

// ============================================================
//  FUNGSI: Inisialisasi LCD
//  Dipanggil satu kali di setup()
// ============================================================
void lcdInit() {
  lcd.init();
  lcd.backlight();  // Nyalakan lampu belakang LCD

  // Daftarkan semua karakter kustom ke memori LCD
  // Parameter: (slot 0-7, array karakter)
  lcd.createChar(0, charHeart);
  lcd.createChar(1, charBarFull);
  lcd.createChar(2, charBarEmpty);
  lcd.createChar(3, charCheck);
  lcd.createChar(4, charUp);
  lcd.createChar(5, charDown);
  lcd.createChar(6, charDot);
}

// ============================================================
//  FUNGSI: Layar Selamat Datang
//  Tampil saat pertama kali alat dinyalakan
//
//  Tampilan di LCD:
//  +----------------+
//  |[hati] LUMIGLU|
//  | ESP32+MAX30102 |
//  +----------------+
// ============================================================
void lcdShowWelcome() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(byte(0));        // Ikon hati
  lcd.print("LUMIGLU");
  lcd.setCursor(0, 1);
  lcd.print(" ESP32+MAX30102");
  delay(2500);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Deteksi Glukosa");
  lcd.setCursor(0, 1);
  lcd.print("  Non-Invasif");
  delay(2000);
}

// ============================================================
//  FUNGSI: Layar "Tempelkan Jari"
//  Tampil saat menunggu jari ditempelkan ke sensor
//
//  Tampilan di LCD:
//  +----------------+
//  |Tempel jari ke  |
//  |sensor...  [dot]|
//  +----------------+
// ============================================================
void lcdShowWaitFinger() {
  static int dotCount = 0;
  static unsigned long lastDotTime = 0;

  if (millis() - lastDotTime > 600) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tempel jari ke");
    lcd.setCursor(0, 1);
    lcd.print("sensor");

    // Animasi titik berjalan
    dotCount = (dotCount + 1) % 4;
    for (int i = 0; i < dotCount; i++) {
      lcd.write(byte(6));  // Ikon titik
    }

    lastDotTime = millis();
  }
}

// ============================================================
//  FUNGSI: Layar Progress Pengukuran
//  Tampil saat sedang mengumpulkan sampel PPG
//
//  Parameter:
//    progress = 0..100 (persentase selesai)
//
//  Tampilan di LCD:
//  +----------------+
//  |Mengukur...  72%|
//  |[====       ]   |
//  +----------------+
// ============================================================
void lcdShowProgress(int progress) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mengukur...");
  lcd.setCursor(12, 0);
  if (progress < 10)  lcd.print(" ");
  if (progress < 100) lcd.print(" ");
  lcd.print(progress);
  lcd.print("%");

  // Progress bar: 14 karakter lebar bar
  lcd.setCursor(0, 1);
  lcd.write(byte(2));  // Kurung kiri (pakai karakter batang kosong kiri)
  int filled = (progress * 13) / 100;  // Konversi 0-100% ke 0-13 karakter
  for (int i = 0; i < 13; i++) {
    if (i < filled) {
      lcd.write(byte(1));  // Batang penuh
    } else {
      lcd.print(" ");      // Kosong
    }
  }
  lcd.write(byte(2));  // Kurung kanan
}

// ============================================================
//  FUNGSI: Layar "Memproses Data"
//  Tampil sebentar saat menghitung estimasi dari fitur PPG
//
//  Tampilan di LCD:
//  +----------------+
//  |Memproses data..|
//  |Harap tunggu... |
//  +----------------+
// ============================================================
void lcdShowProcessing() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Memproses data..");
  lcd.setCursor(0, 1);
  lcd.print("Harap tunggu...");
  delay(1500);
}

// ============================================================
//  FUNGSI: Layar Hasil Gula Darah (Layar Utama)
//  Ini layar paling penting — menampilkan angka estimasi
//
//  Parameter:
//    glucose  = nilai estimasi dalam mg/dL
//    category = string kategori ("NORMAL", "RENDAH", dll)
//    icon     = nomor karakter ikon (3=ceklis, 4=naik, 5=turun)
//
//  Contoh tampilan untuk 89 mg/dL normal:
//  +----------------+
//  |Gula: 89 mg/dL  |
//  |[v] NORMAL      |
//  +----------------+
//
//  Contoh tampilan untuk 145 mg/dL tinggi:
//  +----------------+
//  |Gula:145 mg/dL  |
//  |[^] TINGGI      |
//  +----------------+
// ============================================================
void lcdShowGlucoseResult(float glucose, const char* category, byte icon) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Gula:");

  // Format angka: rata kanan di kolom 5-7
  int glucoseInt = (int)glucose;
  if (glucoseInt < 100) lcd.print(" ");   // Padding supaya rata
  if (glucoseInt < 10)  lcd.print(" ");
  lcd.print(glucoseInt);
  lcd.print(" mg/dL");

  lcd.setCursor(0, 1);
  lcd.write(icon);         // Ikon status
  lcd.print(" ");
  lcd.print(category);
}

// ============================================================
//  FUNGSI: Layar Detail Fitur PPG
//  Menampilkan data mentah PPG — berguna untuk debugging
//  dan kalibrasi manual
//
//  Tampilan di LCD (berganti-ganti):
//  Slide 1:
//  +----------------+
//  |HR: 76 bpm      |
//  |SpO2: 97.8 %    |
//  +----------------+
//  Slide 2:
//  +----------------+
//  |Ratio R: 0.7340 |
//  |Amp: 4182       |
//  +----------------+
// ============================================================
void lcdShowDetails(float hr, float spo2, float ratio, float amplitude) {
  // Slide 1: Heart Rate dan SpO2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HR: ");
  lcd.print((int)hr);
  lcd.print(" bpm");
  lcd.setCursor(0, 1);
  lcd.print("SpO2: ");
  lcd.print(spo2, 1);   // 1 angka di belakang koma
  lcd.print(" %");
  delay(DISPLAY_DELAY_MS);

  // Slide 2: Rasio R dan Amplitudo (data PPG mentah)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ratio R:");
  lcd.print(ratio, 4);  // 4 angka di belakang koma
  lcd.setCursor(0, 1);
  lcd.print("Amp: ");
  lcd.print((int)amplitude);
  delay(DISPLAY_DELAY_MS);
}

// ============================================================
//  FUNGSI: Layar Peringatan/Saran
//  Ditampilkan setelah hasil, memberikan saran tindakan
//
//  Parameter:
//    glucose = nilai estimasi untuk menentukan saran
//
//  Tampilan bervariasi sesuai kategori hasil.
// ============================================================
void lcdShowAdvice(float glucose) {
  lcd.clear();

  if (glucose < GLUCOSE_HIPO) {
    // Hipoglikemia (<70) — perlu tindakan segera
    lcd.setCursor(0, 0);
    lcd.print("! Segera makan/");
    lcd.setCursor(0, 1);
    lcd.print("minum yg manis!");
  }
  else if (glucose < GLUCOSE_NORMAL) {
    // Normal (70–139) — informasi positif
    lcd.setCursor(0, 0);
    lcd.print("GDS Normal :)");
    lcd.setCursor(0, 1);
    lcd.print("Jaga pola makan");
  }
  else if (glucose < GLUCOSE_PREDIABET) {
    // Prediabetes (140–199) — peringatan moderat
    lcd.setCursor(0, 0);
    lcd.print("Risiko Pradiabet");
    lcd.setCursor(0, 1);
    lcd.print("kurangi manis");
  }
  else {
    // Diabetes (>=200) — peringatan serius
    lcd.setCursor(0, 0);
    lcd.print("GDS>=200:Periksa");
    lcd.setCursor(0, 1);
    lcd.print("Segera ke dokter");
  }

  delay(DISPLAY_DELAY_MS);
}

// ============================================================
//  FUNGSI: Layar Standby (Menunggu Tombol)
//  Ditampilkan di antara pengukuran, menunggu tombol ditekan
//
//  Tampilan di LCD:
//  +----------------+
//  |[hati] LUMIGLU  |
//  |Tekan tombol... |
//  +----------------+
// ============================================================
void lcdShowStandby() {
  static unsigned long lastBlink = 0;
  static bool blinkState = true;

  if (millis() - lastBlink > 800) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.write(byte(0));   // Ikon hati
    lcd.print(" LUMIGLU");
    lcd.setCursor(0, 1);
    if (blinkState) {
      lcd.print("Tekan tombol...");
    } else {
      lcd.print("               ");  // Kosong (efek berkedip)
    }
    blinkState = !blinkState;
    lastBlink = millis();
  }
}

// ============================================================
//  FUNGSI: Layar Siap Ulang
//  Tampil di akhir siklus, sebelum mulai pengukuran baru
//
//  Tampilan di LCD:
//  +----------------+
//  |Tekan reset atau|
//  |tunggu 5 detik..|
//  +----------------+
// ============================================================
void lcdShowReady() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pengukuran ulang");
  lcd.setCursor(0, 1);
  lcd.print("tekan tombol...");
}

// ============================================================
//  FUNGSI: Layar Error
//  Ditampilkan kalau sensor tidak terdeteksi
// ============================================================
void lcdShowError(const char* msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("! ERROR !");
  lcd.setCursor(0, 1);
  lcd.print(msg);
}

#endif // LCD_DISPLAY_H
