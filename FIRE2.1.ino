#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#include <HardwareSerial.h>
#include <DFRobot_RTU.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================================================
// OLED
// ======================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ======================================================
// NODE & WIFI & TELEGRAM
// ======================================================
#define NODE_ID "ESP32_NODE_01"

const char* ssid = "NAMA_WIFI";
const char* password = "PW_WIFI";

#define BOT_TOKEN "ISI_TOKEN_BOT"
#define CHAT_ID "ISI_CHAT_ID"
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ======================================================
// THRESHOLDS 
// ============================ ==========================
#define MQ2_PIN 34
#define MQ2_WASPADA   282
#define MQ2_BAHAYA    708
#define MQ2_DARURAT   2238

#define TEMP_WASPADA  35
#define TEMP_BAHAYA   45
#define TEMP_DARURAT  55

// ======================================================
// OUTPUT (RELAY ACTIVE LOW & MOSFET BUZZER)
// ======================================================
#define BUZZER_PIN 4 // MOSFET (Active HIGH)

#define RELAY_ON  LOW
#define RELAY_OFF HIGH

#define RELAY_IN2_GREEN  27 // IN4 Relay -> LED Hijau
#define RELAY_IN3_YELLOW 14 // IN3 Relay -> LED Kuning
#define RELAY_IN4_RED    26 // IN2 Relay -> LED Merah

// ======================================================
// RS485 SHT20
// ======================================================
HardwareSerial SerialRS485(2);
#define RXD2 16
#define TXD2 17
DFRobot_RTU Modbus_Master(&SerialRS485);

uint16_t regTemp;
uint16_t regHum;

// ======================================================
// VARIABLES
// ======================================================
unsigned long lastRead = 0;
unsigned long previousBuzzerMillis = 0;
unsigned long previousLedMillis = 0;

const long interval = 3000;

float temperature = 0;
float humidity = 0;

float suhuKalibrasi = 0;
float humiKalibrasi = 0;

String statusGas = "AMAN";
String statusTemp = "NORMAL";
String finalStatus = "AMAN";
String lastTelegramStatus = "";

int buzzerInterval = 0;
bool ledState = false;

// Function Declaration
int readMQ2Average();
void readPLC();
String getFinalStatus(String gas, String temp);
void controlOutput(String status);
void buzzerBeep(unsigned long currentMillis, int onTime);
void sendTelegramAlert(String levelBahaya, int mq2Value);

// ======================================================
// SETUP
// ======================================================
void setup() {
  Serial.begin(115200);

  pinMode(MQ2_PIN, INPUT);

  // Buzzer MOSFET
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Relay Pins
  pinMode(RELAY_IN2_GREEN, OUTPUT);
  pinMode(RELAY_IN3_YELLOW, OUTPUT);
  pinMode(RELAY_IN4_RED, OUTPUT);

  // Matikan semua LED relay saat baru dinyalakan
  digitalWrite(RELAY_IN2_GREEN, RELAY_OFF);
  digitalWrite(RELAY_IN3_YELLOW, RELAY_OFF);
  digitalWrite(RELAY_IN4_RED, RELAY_OFF);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FAILED");
    while (1);
  }

  // RS485
  SerialRS485.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // WiFi
  WiFi.begin(ssid, password);
  client.setInsecure();

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");

  // Telegram Start Notification
  bot.sendMessage(
    CHAT_ID,
    "✅ SYSTEM STARTED\n📡 Node : " + String(NODE_ID),
    "Markdown"
  );
}

// ======================================================
// LOOP
// ======================================================
void loop() {
  int mq2Value = readMQ2Average();

  if (mq2Value >= MQ2_DARURAT) {
    statusGas = "DARURAT";
  } else if (mq2Value >= MQ2_BAHAYA) {
    statusGas = "BAHAYA";
  } else if (mq2Value >= MQ2_WASPADA) {
    statusGas = "WASPADA";
  } else {
    statusGas = "AMAN";
  }

  if (millis() - lastRead >= interval) {
    lastRead = millis();
    readPLC();

    if (suhuKalibrasi >= TEMP_DARURAT) {
      statusTemp = "DARURAT";
    } else if (suhuKalibrasi >= TEMP_BAHAYA) {
      statusTemp = "BAHAYA";
    } else if (suhuKalibrasi >= TEMP_WASPADA) {
      statusTemp = "WASPADA";
    } else {
      statusTemp = "NORMAL";
    }
  }

  finalStatus = getFinalStatus(statusGas, statusTemp);

  if (finalStatus != "AMAN" && finalStatus != lastTelegramStatus) {
    sendTelegramAlert(finalStatus, mq2Value);
    lastTelegramStatus = finalStatus;
  }

  if (finalStatus == "AMAN") {
    lastTelegramStatus = "";
  }

  controlOutput(finalStatus);

  // OLED Display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("FIRE MONITOR");

  display.setCursor(0, 12);
  display.print("MQ2 : ");
  display.println(mq2Value);

  display.setCursor(0, 24);
  display.print("Temp: ");
  display.print(suhuKalibrasi);
  display.println(" C");

  display.setCursor(0, 36);
  display.print("Hum : ");
  display.print(humiKalibrasi);
  display.println(" %");

  display.setCursor(0, 50);
  display.print("Stat: ");
  display.println(finalStatus);

  display.display();

  // Serial Monitor Output
  Serial.println("========================");
  Serial.print("MQ2 : ");
  Serial.println(mq2Value);
  Serial.print("Status Gas : ");
  Serial.println(statusGas);
  Serial.print("Temp : ");
  Serial.print(suhuKalibrasi);
  Serial.print(" C | ");
  Serial.println(statusTemp);
  Serial.print("Hum : ");
  Serial.print(humiKalibrasi);
  Serial.println(" %");
  Serial.print("FINAL STATUS : ");
  Serial.println(finalStatus);

  delay(300);
}

// ======================================================
// FILTER MQ2
// ======================================================
int readMQ2Average() {
  long total = 0;
  for (int i = 0; i < 5; i++) {
    total += analogRead(MQ2_PIN);
    delay(20);
  }
  return total / 5;
}

// ======================================================
// READ SHT20 (SAMA SEPERTI PROGRAM LAMA)
// ======================================================
// void readPLC() {
//   regTemp = Modbus_Master.readInputRegister(1, 1);
//   delay(20);

//   regHum = Modbus_Master.readInputRegister(1, 2);
//   delay(20);

//   temperature = regTemp / 10.0;
//   humidity = regHum / 10.0;

//   suhuKalibrasi = (suhuKalibrasi + (temperature - 3.5)) / 2;
//   humiKalibrasi = (humiKalibrasi + (humidity + 9.8)) / 2;
// }

void readPLC() {

  regTemp = Modbus_Master.readInputRegister(1, 1);
  delay(20);

  regHum = Modbus_Master.readInputRegister(1, 2);
  delay(20);

  // Pembacaan sensor
  temperature = regTemp / 10.0;
  humidity = regHum / 10.0;

  // Kalibrasi
  suhuKalibrasi = (suhuKalibrasi + (temperature - 3.5)) / 2;
  humiKalibrasi = (humiKalibrasi + (humidity + 9.8)) / 2;

  // Format hasil
  suhuKalibrasi = round(suhuKalibrasi * 10) / 10.0;  // 1 angka desimal
  humiKalibrasi = round(humiKalibrasi);                // bilangan bulat
}

// ======================================================
// FINAL STATUS
// ======================================================
String getFinalStatus(String gas, String temp) {
  if (gas == "DARURAT" || temp == "DARURAT") {
    return "DARURAT";
  } else if (gas == "BAHAYA" || temp == "BAHAYA") {
    return "BAHAYA";
  } else if (gas == "WASPADA" || temp == "WASPADA") {
    return "WASPADA";
  }
  return "AMAN";
}

// ======================================================
// OUTPUT CONTROL (RELAY ACTIVE LOW & MOSFET BUZZER)
// ======================================================
void controlOutput(String status) {
  unsigned long currentMillis = millis();

  if (status == "AMAN") {
    digitalWrite(RELAY_IN2_GREEN, RELAY_ON);   // LED Hijau ON
    digitalWrite(RELAY_IN3_YELLOW, RELAY_OFF); // LED Kuning OFF
    digitalWrite(RELAY_IN4_RED, RELAY_OFF);    // LED Merah OFF
    digitalWrite(BUZZER_PIN, LOW);
  }
  else if (status == "WASPADA") {
    digitalWrite(RELAY_IN2_GREEN, RELAY_OFF);
    digitalWrite(RELAY_IN3_YELLOW, RELAY_ON);  // LED Kuning ON
    digitalWrite(RELAY_IN4_RED, RELAY_OFF);

    buzzerInterval = 10000;
    buzzerBeep(currentMillis, 300);
  }
  else if (status == "BAHAYA") {
    digitalWrite(RELAY_IN2_GREEN, RELAY_OFF);
    digitalWrite(RELAY_IN3_YELLOW, RELAY_OFF);
    digitalWrite(RELAY_IN4_RED, RELAY_ON);     // LED Merah ON

    buzzerInterval = 5000;
    buzzerBeep(currentMillis, 700);
  }
  else if (status == "DARURAT") {
    digitalWrite(RELAY_IN2_GREEN, RELAY_OFF);
    digitalWrite(RELAY_IN3_YELLOW, RELAY_OFF);

    // LED Merah Berkedip
    if (currentMillis - previousLedMillis >= 500) {
      previousLedMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(RELAY_IN4_RED, ledState ? RELAY_ON : RELAY_OFF);
    }

    buzzerInterval = 2000;
    buzzerBeep(currentMillis, 1200);
  }
}

// ======================================================
// BUZZER
// ======================================================
void buzzerBeep(unsigned long currentMillis, int onTime) {
  if (currentMillis - previousBuzzerMillis >= buzzerInterval) {
    previousBuzzerMillis = currentMillis;
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onTime);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ======================================================
// TELEGRAM ALERT
// ======================================================
void sendTelegramAlert(String levelBahaya, int mq2Value) {
  String detection = "";

  if (statusGas != "AMAN" && statusTemp != "NORMAL") {
    detection = "GAS & TEMPERATURE";
  } else if (statusGas != "AMAN") {
    detection = "GAS / SMOKE";
  } else if (statusTemp != "NORMAL") {
    detection = "TEMPERATURE";
  }

  String msg = "";
  msg += "🚨 *KEBAKARAN TERDETEKSI* 🚨\n\n";
  msg += "📡 *Node* : " + String(NODE_ID) + "\n";
  msg += "🧯 *Deteksi* : " + detection + "\n";
  msg += "⚠️ *Status Final* : " + levelBahaya + "\n\n";
  msg += "💨 *Gas* : " + String(mq2Value) + "\n";
  msg += "🌡️ *Suhu* : " + String(suhuKalibrasi) + " C\n";
  msg += "💧 *Kelembapan* : " + String(humiKalibrasi) + " %\n\n";

  if (levelBahaya == "WASPADA") {
    msg += "🟡 *KONDISI WASPADA*";
  } else if (levelBahaya == "BAHAYA") {
    msg += "🟠 *KONDISI BAHAYA*";
  } else if (levelBahaya == "DARURAT") {
    msg += "🔴 *KONDISI DARURAT*";
  }

  bot.sendMessage(CHAT_ID, msg, "Markdown");
}