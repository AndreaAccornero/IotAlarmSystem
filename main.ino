#define PRESSURE_SENSOR_PIN 33 // Definisce il pin collegato al sensore di pressione
#define LED_BUILTIN 2          // Pin del LED integrato (di solito il pin 2 per ESP32)
#define SPEAKER_PIN 32         // Pin collegato allo speaker
#define PRESSURE_THRESHOLD 4095 // Soglia per attivare il lampeggio e il suono

void setup() {
  // Inizializzazione del monitor seriale per il debug
  Serial.begin(115200);
  // Configura il pin del sensore come ingresso
  pinMode(PRESSURE_SENSOR_PIN, INPUT);
  // Configura il LED integrato come uscita
  pinMode(LED_BUILTIN, OUTPUT);
  // Configura lo speaker come uscita
  pinMode(SPEAKER_PIN, OUTPUT);
}

void loop() {
  // Legge il valore dal sensore di pressione
  int sensorValue = analogRead(PRESSURE_SENSOR_PIN);

  // Stampa il valore letto sul monitor seriale
  Serial.print("Valore del sensore: ");
  Serial.println(sensorValue);

  // Controlla se il valore supera o è uguale alla soglia
  if (sensorValue >= PRESSURE_THRESHOLD) {
    // Lampeggia il LED integrato
    digitalWrite(LED_BUILTIN, HIGH); // Accende il LED
    tone(SPEAKER_PIN, 2000);         // Suona lo speaker a 1000 Hz
    delay(250);                      // Aspetta 250 ms
    digitalWrite(LED_BUILTIN, LOW);  // Spegne il LED
    noTone(SPEAKER_PIN);             // Ferma il suono dello speaker
    delay(250);                      // Aspetta 250 ms
  } else {
    // Assicura che il LED sia spento e lo speaker sia muto se il valore è sotto la soglia
    digitalWrite(LED_BUILTIN, LOW);
    noTone(SPEAKER_PIN);
  }

  // Piccola pausa prima della prossima lettura
  delay(500);
}
