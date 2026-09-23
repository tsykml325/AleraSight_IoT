#include <HardwareSerial.h>
#include <DFRobot_RTU.h>

// ======================================================
// RELAY
// ======================================================
// Sesuaikan dengan pin IN relay yang digunakan
#define RELAY_PIN 27

// Relay aktif LOW
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ======================================================
// RS485 SHT20
// ======================================================
HardwareSerial SerialRS485(2);

#define RXD2 16
#define TXD2 17

DFRobot_RTU Modbus_Master(&SerialRS485);

// ======================================================
// THRESHOLD SUHU
// ======================================================
#define TEMP_WASPADA 35.0

// ======================================================
// VARIABLE
// ======================================================
uint16_t regTemp;

float temperature = 0;
float suhuKalibrasi = 0;

unsigned long lastRead = 0;
const unsigned long interval = 3000;

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(115200);

  // Relay
  pinMode(RELAY_PIN, OUTPUT);

  // Kondisi awal:
  // Relay OFF -> kontak NC aktif -> LED KUNING
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // RS485
  SerialRS485.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("================================");
  Serial.println("   TEST LED BERDASARKAN SUHU");
  Serial.println("================================");
  Serial.println("LED Hijau = NO");
  Serial.println("LED Kuning = NC");
  Serial.println();
}

// ======================================================
// LOOP
// ======================================================
void loop() {

  if (millis() - lastRead >= interval) {

    lastRead = millis();

    // ================================================
    // BACA SUHU SHT20
    // ================================================
    regTemp = Modbus_Master.readInputRegister(1, 1);

    delay(20);

    temperature = regTemp / 10.0;

    // Kalibrasi suhu sesuai program sebelumnya
    suhuKalibrasi = temperature - 3.5;

    // ================================================
    // TAMPILKAN DATA
    // ================================================
    Serial.println("--------------------------------");

    Serial.print("Suhu Sensor     : ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Suhu Kalibrasi  : ");
    Serial.print(suhuKalibrasi);
    Serial.println(" C");

    // ================================================
    // KONTROL RELAY
    // ================================================
    if (suhuKalibrasi >= TEMP_WASPADA) {

      // =================================================
      // SUHU >= 35 C
      // RELAY ON
      // Kontak NO terhubung
      // LED HIJAU MENYALA
      // =================================================

      digitalWrite(RELAY_PIN, RELAY_ON);

      Serial.println("Status : WASPADA");
      Serial.println("Relay  : ON");
      Serial.println("LED    : HIJAU (NO)");

    } else {

      // =================================================
      // SUHU < 35 C
      // RELAY OFF
      // Kontak NC terhubung
      // LED KUNING MENYALA
      // =================================================

      digitalWrite(RELAY_PIN, RELAY_OFF);

      Serial.println("Status : NORMAL");
      Serial.println("Relay  : OFF");
      Serial.println("LED    : KUNING (NC)");
    }
  }
}