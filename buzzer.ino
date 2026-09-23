#define BUZZER_PIN 4

void setup() {

  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("TEST BUZZER 12V");
}

void loop() {

  Serial.println("BUZZER ON");

  digitalWrite(BUZZER_PIN, HIGH);

  delay(3000);

  Serial.println("BUZZER OFF");

  digitalWrite(BUZZER_PIN, LOW);

  delay(3000);
}