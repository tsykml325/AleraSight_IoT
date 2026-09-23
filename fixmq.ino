// ======================================================
// MQ2 ESP32 - STABIL & RESPONSIF
// ======================================================

#define MQ2_PIN 34

// ================== THRESHOLD ==================
//#define WASPADA_LEVEL  150
//#define BAHAYA_LEVEL   250
//#define DARURAT_LEVEL  400

// SESUDAH (hasil kalibrasi ulang dari analisis kita)
#define WASPADA_LEVEL   308
#define BAHAYA_LEVEL    708
#define DARURAT_LEVEL   2238

#define RESET_LEVEL    285

// ================== VARIABLE ==================
unsigned long safeTimer = 0;

String statusGas = "AMAN";

// ======================================================
void setup() {

  Serial.begin(115200);

  pinMode(MQ2_PIN, INPUT);

  Serial.println("MQ2 START");
}

// ======================================================
void loop() {

  int mq2Value = readMQ2Average();

  // ======================================================
  // DARURAT
  // ======================================================
  if (mq2Value >= DARURAT_LEVEL) {

    statusGas = "DARURAT";

    safeTimer = millis();
  }

  // ======================================================
  // BAHAYA
  // ======================================================
  else if (mq2Value >= BAHAYA_LEVEL) {

    statusGas = "BAHAYA";

    safeTimer = millis();
  }

  // ======================================================
  // WASPADA
  // ======================================================
  else if (mq2Value >= WASPADA_LEVEL) {

    statusGas = "WASPADA";

    safeTimer = millis();
  }

  // ======================================================
  // AMAN
  // ======================================================
  else if (mq2Value < RESET_LEVEL) {

    // Delay aman 3 detik
    if (millis() - safeTimer > 3000) {

      statusGas = "AMAN";
    }
  }

  // ======================================================
  // SERIAL MONITOR
  // ======================================================
  Serial.print("MQ2 : ");
  Serial.print(mq2Value);
  Serial.print(" ppm");

  Serial.print("| Status : ");
  Serial.println(statusGas);

  delay(500);
}

// ======================================================
// FILTERING MQ2
// ======================================================
int readMQ2Average() {

  long total = 0;

  for (int i = 0; i < 5; i++) {

    total += analogRead(MQ2_PIN);

    delay(20);
  }

  return total / 5;
}