#define PRESSURE_SENSOR_PIN 33 // Definisce il pin collegato al sensore di pressione
#define LED_PIN 13             // Definisce il pin collegato al LED
#define PRESSURE_THRESHOLD 4095 // Soglia per pressione massima

void setup() {
  // Inizializzazione del monitor seriale per il debug
  Serial.begin(115200);
  // Configura il pin del sensore come ingresso
  pinMode(PRESSURE_SENSOR_PIN, INPUT);
  // Configura il pin del LED come uscita
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Legge il valore dal sensore di pressione
  int sensorValue = analogRead(PRESSURE_SENSOR_PIN);

  // Controlla se il valore è maggiore o uguale alla soglia
  if (sensorValue >= PRESSURE_THRESHOLD) {
    Serial.println("Pressione massima rilevata!");
    // Fa lampeggiare il LED
    digitalWrite(LED_PIN, HIGH); // Accende il LED
    delay(500);                  // Aspetta 500 ms
    digitalWrite(LED_PIN, LOW);  // Spegne il LED
    delay(500);                  // Aspetta 500 ms
  } else {
    // Se il valore è sotto la soglia, assicura che il LED sia spento
    digitalWrite(LED_PIN, LOW);
    Serial.println("Nessuna pressione rilevata o sotto soglia.");
  }

  // Piccola pausa per evitare sovraccarico di letture
  delay(100);
}
