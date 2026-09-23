#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <PubSubClient.h>

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

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1 
);

// ======================================================
// NODE
// ======================================================
#define NODE_ID "ESP32_NODE_01"

// ======================================================
// WIFI
// ======================================================
const char* ssid = "syahla";
const char* password = "tasya0302";

// ======================================================
// TELEGRAM
// ======================================================
#define BOT_TOKEN "8138519772:AAGNX8LlE7zywjtXMPCALyFNgGfSZIPp0IE"
#define CHAT_ID "7884069807"
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ======================================================
// MQTT (HiveMQ)
// ======================================================
const char* mqtt_host = "ac33bb52b0b8462f89b158f213570c56.s1.eu.hivemq.cloud"; // ganti host kamu
const int mqtt_port = 8883;
const char* mqtt_user = "homeguard-device";
const char* mqtt_pass = "Adminalera_123";
const char* mqtt_topic = "homeguard/sensor";

WiFiClientSecure espClientMQTT;
PubSubClient mqttClient(espClientMQTT);

// ======================================================
// MQ2
// ======================================================
#define MQ2_PIN 34

#define MQ2_WASPADA   150
#define MQ2_BAHAYA    250
#define MQ2_DARURAT   400

// ======================================================
// TEMPERATURE
// ======================================================
#define TEMP_WASPADA   35
#define TEMP_BAHAYA    45
#define TEMP_DARURAT   55

// ======================================================
// OUTPUT
// ======================================================
#define BUZZER_PIN 4
#define LED_GREEN  27
#define LED_YELLOW 14
#define LED_RED    26

// ======================================================
// RS485
// ======================================================
HardwareSerial SerialRS485(2);
#define RXD2 16
#define TXD2 17
DFRobot_RTU Modbus_Master(&SerialRS485);

uint16_t regTemp;
uint16_t regHum;

// ======================================================
// VARIABLE
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

// ======================================================
// SETUP
// ======================================================
void setup() {

  Serial.begin(115200);

  // ======================================================
  // MQ2
  // ======================================================
  pinMode(MQ2_PIN, INPUT);

  // ======================================================
  // OUTPUT
  // ======================================================
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  // ======================================================
  // OLED
  // ======================================================
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C)) {

    Serial.println("OLED FAILED");

    while (1);
  }

  // ======================================================
  // RS485
  // ======================================================
  SerialRS485.begin(
    9600,
    SERIAL_8N1,
    RXD2,
    TXD2
  );

  // ======================================================
  // WIFI
  // ======================================================
  WiFi.begin(ssid, password);

  client.setInsecure();

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  // ======================================================
  // TELEGRAM START
  // ======================================================
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

  if (!mqttClient.connected()) {
  connectMQTT();
  }
  mqttClient.loop();

  // ======================================================
  // MQ2
  // ======================================================
  int mq2Value = readMQ2Average();

  if (mq2Value >= MQ2_DARURAT) {

    statusGas = "DARURAT";
  }

  else if (mq2Value >= MQ2_BAHAYA) {

    statusGas = "BAHAYA";
  }

  else if (mq2Value >= MQ2_WASPADA) {

    statusGas = "WASPADA";
  }

  else {

    statusGas = "AMAN";
    publishSensorData(mq2Value);
  }

  // ======================================================
  // READ SENSOR
  // ======================================================
  if (millis() - lastRead >= interval) {

    lastRead = millis();

    readPLC();

    // ======================================================
    // TEMPERATURE STATUS
    // ======================================================
    if (suhuKalibrasi >= TEMP_DARURAT) {

      statusTemp = "DARURAT";
    }

    else if (suhuKalibrasi >= TEMP_BAHAYA) {

      statusTemp = "BAHAYA";
    }

    else if (suhuKalibrasi >= TEMP_WASPADA) {

      statusTemp = "WASPADA";
    }

    else {

      statusTemp = "NORMAL";
    }
  }

  // ======================================================
  // FINAL STATUS
  // ======================================================
  finalStatus = getFinalStatus(
                  statusGas,
                  statusTemp
                );

  // ======================================================
  // TELEGRAM ALERT
  // ======================================================
  if (finalStatus != "AMAN" &&
      finalStatus != lastTelegramStatus) {

    sendTelegramAlert(
      finalStatus,
      mq2Value
    );

    lastTelegramStatus = finalStatus;
  }

  // ======================================================
  // RESET STATUS
  // ======================================================
  if (finalStatus == "AMAN") {

    lastTelegramStatus = "";
  }

  // ======================================================
  // OUTPUT
  // ======================================================
  controlOutput(finalStatus);

  // ======================================================
  // OLED
  // ======================================================
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

  // ======================================================
  // SERIAL MONITOR
  // ======================================================
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
  connectMQTT();

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
// MQTT CONNECT
// ======================================================
void connectMQTT() {

  espClientMQTT.setInsecure();
  mqttClient.setServer(mqtt_host, mqtt_port);

  if (!mqttClient.connected()) {

    Serial.println("Connecting to MQTT...");

    if (mqttClient.connect(NODE_ID, mqtt_user, mqtt_pass)) {

      Serial.println("MQTT connected!");
    } else {

      Serial.print("MQTT failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

// ======================================================
// MQTT PUBLISH
// ======================================================
void publishSensorData(int mq2Value) {

  String payload = "{";
  payload += "\"device_id\":\"" + String(NODE_ID) + "\",";
  payload += "\"temperature\":" + String(suhuKalibrasi) + ",";
  payload += "\"humidity\":" + String(humiKalibrasi) + ",";
  payload += "\"smoke_level\":" + String(mq2Value) + ",";
  payload += "\"status\":\"" + finalStatus + "\"";
  payload += "}";

  mqttClient.publish(mqtt_topic, payload.c_str());

  Serial.println("MQTT Published: " + payload);
}

// ======================================================
// READ SHT20
// ======================================================
void readPLC() {

  regTemp =
    Modbus_Master.readInputRegister(1, 1);

  delay(20);

  regHum =
    Modbus_Master.readInputRegister(1, 2);

  delay(20);

  temperature = regTemp / 10.0;
  humidity = regHum / 10.0;

  suhuKalibrasi =
    (suhuKalibrasi +
     (temperature - 3.5)) / 2;

  humiKalibrasi =
    (humiKalibrasi +
     (humidity + 9.8)) / 2;
}

// ======================================================
// FINAL STATUS
// ======================================================
String getFinalStatus(
  String gas,
  String temp
) {

  if (gas == "DARURAT" ||
      temp == "DARURAT") {

    return "DARURAT";
  }

  else if (gas == "BAHAYA" ||
           temp == "BAHAYA") {

    return "BAHAYA";
  }

  else if (gas == "WASPADA" ||
           temp == "WASPADA") {

    return "WASPADA";
  }

  return "AMAN";
}

// ======================================================
// OUTPUT CONTROL
// ======================================================
void controlOutput(String status) {

  unsigned long currentMillis =
    millis();

  // ======================================================
  // AMAN
  // ======================================================
  if (status == "AMAN") {

    digitalWrite(LED_GREEN, HIGH);

    digitalWrite(LED_YELLOW, LOW);

    digitalWrite(LED_RED, LOW);

    digitalWrite(BUZZER_PIN, LOW);
  }

  // ======================================================
  // WASPADA
  // ======================================================
  else if (status == "WASPADA") {

    digitalWrite(LED_GREEN, LOW);

    digitalWrite(LED_YELLOW, HIGH);

    digitalWrite(LED_RED, LOW);

    buzzerInterval = 10000;

    buzzerBeep(currentMillis, 300);
  }

  // ======================================================
  // BAHAYA
  // ======================================================
  else if (status == "BAHAYA") {

    digitalWrite(LED_GREEN, LOW);

    digitalWrite(LED_YELLOW, LOW);

    digitalWrite(LED_RED, HIGH);

    buzzerInterval = 5000;

    buzzerBeep(currentMillis, 700);
  }

  // ======================================================
  // DARURAT
  // ======================================================
  else if (status == "DARURAT") {

    digitalWrite(LED_GREEN, LOW);

    digitalWrite(LED_YELLOW, LOW);

    // ======================================================
    // LED BERKEDIP
    // ======================================================
    if (currentMillis -
        previousLedMillis >= 500) {

      previousLedMillis =
        currentMillis;

      ledState = !ledState;

      digitalWrite(
        LED_RED,
        ledState
      );
    }

    buzzerInterval = 2000;

    buzzerBeep(currentMillis, 1200);
  }
}

// ======================================================
// BUZZER
// ======================================================
void buzzerBeep(
  unsigned long currentMillis,
  int onTime
) {

  if (currentMillis -
      previousBuzzerMillis >=
      buzzerInterval) {

    previousBuzzerMillis =
      currentMillis;

    digitalWrite(BUZZER_PIN, HIGH);

    delay(onTime);

    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ======================================================
// TELEGRAM ALERT
// ======================================================
void sendTelegramAlert(
  String levelBahaya,
  int mq2Value
) {

  String detection = "";

  // ======================================================
  // DETEKSI SUMBER
  // ======================================================
  if (statusGas != "AMAN" &&
      statusTemp != "NORMAL") {

    detection =
      "GAS & TEMPERATURE";
  }

  else if (statusGas != "AMAN") {

    detection =
      "GAS / SMOKE";
  }

  else if (statusTemp != "NORMAL") {

    detection =
      "TEMPERATURE";
  }

  // ======================================================
  // MESSAGE
  // ======================================================
  String msg = "";

  msg += "🚨 *KEBAKARAN TERDETEKSI* 🚨\n\n";

  msg += "📡 *Node* : ";
  msg += String(NODE_ID);
  msg += "\n";

  msg += "🧯 *Deteksi* : ";
  msg += detection;
  msg += "\n";

  msg += "⚠️ *Status Final* : ";
  msg += levelBahaya;
  msg += "\n\n";

  msg += "💨 *Gas* : ";
  msg += String(mq2Value);
  msg += "\n";

  msg += "🌡️ *Suhu* : ";
  msg += String(suhuKalibrasi);
  msg += " C\n";

  msg += "💧 *Kelembapan* : ";
  msg += String(humiKalibrasi);
  msg += " %\n\n";

  // ======================================================
  // KONDISI
  // ======================================================
  if (levelBahaya == "WASPADA") {

    msg += "🟡 *KONDISI WASPADA*";
  }

  else if (levelBahaya == "BAHAYA") {

    msg += "🟠 *KONDISI BAHAYA*";
  }

  else if (levelBahaya == "DARURAT") {

    msg += "🔴 *KONDISI DARURAT*";
  }

  // ======================================================
  // SEND TELEGRAM
  // ======================================================
  bot.sendMessage(
    CHAT_ID,
    msg,
    "Markdown"
  );
}