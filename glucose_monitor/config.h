/**
 * ============================================================
 *  config.h  —  FILE KONFIGURASI
 * ============================================================
 *  Semua angka pengaturan ada di sini.
 *  Kalau mau ubah kalibrasi atau threshold, cukup edit file
 *  ini saja, tidak perlu otak-atik kode utama.
 * ============================================================
 */
 
#ifndef CONFIG_H
#define CONFIG_H
 
// ============================================================
//  PIN I2C (ESP32 default, tidak perlu diubah)
// ============================================================
#define PIN_SDA   21    // SDA  → GPIO 21
#define PIN_SCL   22    // SCL  → GPIO 22
 
// ============================================================
//  PIN BUZZER & PUSH BUTTON
//  Buzzer  : GPIO 25 → hubungkan ke + buzzer, GND ke - buzzer
//  Button  : GPIO 26 → hubungkan ke GND (pakai INPUT_PULLUP)
//            Tekan tombol untuk memulai pengukuran manual
// ============================================================
#define PIN_BUZZER   25   // Output buzzer aktif (active buzzer)
#define PIN_BUTTON   26   // Input push button (active LOW)
 
// ============================================================
//  ALAMAT I2C PERANGKAT
//  Kalau LCD tidak tampil, coba ubah LCD_ADDRESS ke 0x3F
// ============================================================
#define LCD_ADDRESS   0x27   // Alamat I2C LCD (biasanya 0x27 atau 0x3F)
#define LCD_COLS      16     // Lebar LCD 16 karakter
#define LCD_ROWS      2      // Tinggi LCD 2 baris
 
// ============================================================
//  PENGATURAN SENSOR MAX30102
// ============================================================
#define LED_BRIGHTNESS   60    // Kecerahan LED sensor (mA) — jangan lebih dari 60
#define SAMPLE_AVERAGE    4    // Rerata sampel (1, 2, 4, 8, 16, 32)
#define SAMPLE_RATE     100    // Frekuensi sampling (Hz)
#define PULSE_WIDTH     411    // Lebar pulsa LED (us) — menentukan resolusi ADC
#define ADC_RANGE     16384    // Range ADC (sesuaikan dengan pulse width 411us)
 
// Jumlah total sampel per satu pengukuran
// 100 Hz × 1 detik = 100 sampel
#define SAMPLE_SIZE    100
 
// Nilai minimum IR untuk mendeteksi jari menempel
#define FINGER_THRESHOLD  50000
 
// Timeout tunggu jari (milidetik)
#define FINGER_TIMEOUT_MS  15000   // 15 detik
 
// ============================================================
//  KOEFISIEN MODEL REGRESI GLUKOSA
//  ============================================================
//  Model: Glucose = COEF_INTERCEPT  +  COEF_IR * MeanIR  +  COEF_R * R
//
//  Diperoleh dari regresi linear berganda terhadap 18 data kalibrasi
//  real menggunakan ESP32 + MAX30102.
//
//  Dataset   : 26 sampel total → 18 digunakan (8 dieksklusi)
//              Eksklusi: Subjek 5,8,9,10 (kurang representatif)
//                        Subjek 11,13,14 (outlier Rasio R)
//                        Subjek 15      (outlier MeanIR)
//  Range Y   : 70–158 mg/dL (puasa & setelah makan)
//  R²        : 0.9346  → model sangat baik  (target ≥ 0.85 ✅)
//  RMSE      : 7.55 mg/dL   → akurasi tinggi (target < 20 mg/dL ✅)
//  MAPE      : 6.03%        → error rata-rata (target < 20% ✅)
//
//  Penjelasan koefisien:
//
//  COEF_R = -178.205339317138
//    Koefisien untuk Rasio Red/IR (X₂).
//    Saat glukosa naik → viskositas darah meningkat → penyerapan
//    cahaya IR berubah relatif terhadap Red → rasio R TURUN.
//    Hubungan terbalik kuat; ini variabel dominan dalam model.
//    (R² rasio saja = 0.891, mendekati model 2-variabel = 0.935)
//
//  COEF_IR = -0.00112366719652
//    Koefisien untuk Mean IR absolut (X₁).
//    Kontribusi kecil namun meningkatkan R² dari 0.891 (R saja)
//    menjadi 0.935 (R + MeanIR). Mencerminkan variasi perfusi
//    antar individu yang tidak tertangkap oleh rasio saja.
//    Nilai negatif: MeanIR besar cenderung pada subjek dengan
//    glukosa lebih rendah dalam dataset ini.
//
//  COEF_INTERCEPT = 390.119366087740
//    Offset/baseline persamaan regresi.
//    INI YANG PERTAMA DIUBAH SAAT REKALIBRASI PERSONAL:
//      → Hasil selalu terlalu tinggi: TURUNKAN COEF_INTERCEPT
//      → Hasil selalu terlalu rendah : NAIKKAN COEF_INTERCEPT
// ============================================================
 
// Formula: Glucose = COEF_INTERCEPT + COEF_IR * MeanIR + COEF_R * R
#define COEF_R            -178.205339317138f   // Koefisien Rasio Red/IR
#define COEF_IR             -0.00112366719652f // Koefisien Mean IR absolut
#define COEF_INTERCEPT     390.119366087740f   // Intercept / offset kalibrasi
 
// ============================================================
//  CATATAN KOMPATIBILITAS DENGAN KODE LAMA
//  Jika kode utama masih menggunakan variabel COEF_A, B, C, D,
//  gunakan mapping di bawah ini:
//
//    COEF_A → ganti dengan COEF_R          (-178.205339317138f)
//    COEF_B → tidak digunakan (set 0.0f)   (fitur AC amplitude
//              tidak cukup berkontribusi dalam dataset ini)
//    COEF_C → tidak digunakan (set 0.0f)   (HR tidak diukur
//              dalam sesi kalibrasi ini)
//    COEF_D → ganti dengan COEF_INTERCEPT  (390.119366087740f)
//
//  Dan tambahkan satu term baru untuk MeanIR:
//    glucose += COEF_IR * meanIR;
// ============================================================
 
// ============================================================
//  BATAS KLASIFIKASI GULA DARAH SEWAKTU / GDS (mg/dL)
//  Sumber: Halodoc / Kemenkes RI — GDS (tidak perlu puasa)
//
//  Kategori:
//    < 70          → Hipoglikemia  (gula darah terlalu rendah, berbahaya)
//    70 – 139      → Normal        (rentang aman GDS)
//    140 – 199     → Prediabetes   (risiko, perlu evaluasi & gaya hidup sehat)
//    ≥ 200         → Indikasi Diabetes (sangat disarankan cek ke dokter)
//
//  CATATAN STANDAR GDS (Gula Darah Sewaktu):
//    • Normal      : GDS < 200 mg/dL (tanpa gejala diabetes)
//    • Hipoglikemia: GDS < 70 mg/dL  (perlu penanganan segera)
//    • Prediabetes : GDS 140–199 mg/dL (risiko, disertai gejala lain)
//    • Diabetes    : GDS ≥ 200 mg/dL  (terutama jika ada gejala diabetes)
//
//  PERBEDAAN vs standar puasa (GDP/fasting):
//    Standar puasa: Normal < 100, Prediabetes 100–125, DM ≥ 126
//    Standar GDS  : Normal < 200, Prediabetes 140–199, DM ≥ 200
//    → Alat ini menggunakan standar GDS karena pengukuran
//      dilakukan sewaktu-waktu tanpa kondisi puasa.
// ============================================================
#define GLUCOSE_HIPO       70.0f    // Di bawah ini = Hipoglikemia
#define GLUCOSE_NORMAL    140.0f    // Di bawah ini = Normal (70–139)
#define GLUCOSE_PREDIABET 200.0f    // Di bawah ini = Prediabetes (140–199)
                                    // Di atas/sama dengan 200 = Indikasi Diabetes
 
// Batas fisiologis untuk clamp hasil (supaya tidak out-of-range)
#define GLUCOSE_MIN_CLAMP  20.0f
#define GLUCOSE_MAX_CLAMP 600.0f
 
// ============================================================
//  PENGATURAN TAMPILAN
// ============================================================
#define DISPLAY_DELAY_MS    3000   // Durasi tiap layar info ditampilkan (ms)
#define MEASURING_ANIM_MS    300   // Kecepatan animasi "mengukur..." (ms)
 
// ============================================================
//  PENGATURAN BUZZER
//  Bunyi berbeda per kategori hasil gula darah:
//    RENDAH  → 2 kali bunyi  (peringatan hipoglikemia)
//    NORMAL  → 1 kali bunyi  (konfirmasi hasil normal)
//    TINGGI  → 3 kali bunyi  (peringatan hiperglikemia)
// ============================================================
#define BUZZER_BEEP_MS     150   // Durasi satu bunyi (ms)
#define BUZZER_PAUSE_MS    200   // Jeda antar bunyi (ms)
 
#endif // CONFIG_H