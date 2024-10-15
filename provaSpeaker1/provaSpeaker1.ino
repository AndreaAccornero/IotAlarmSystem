// Definisci i pin per lo speaker e il LED
const int speakerPin = 25;  // Pin GPIO collegato allo speaker
const int ledPin = 2;       // Pin GPIO collegato al LED

void setup() {
  // Inizializza i pin come output
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // Accendi il LED e suona lo speaker
  digitalWrite(ledPin, HIGH);   // Accendi il LED
  tone(speakerPin, 1000);       // Suona un tono a 1000 Hz
  delay(5000);                   // Suona per 500 ms

  // Spegni il LED e ferma il suono
  digitalWrite(ledPin, LOW);    // Spegni il LED
  noTone(speakerPin);           // Ferma il suono
  delay(5000);                   // Pausa per 500 ms

  // Accendi il LED e suona un tono diverso
  digitalWrite(ledPin, HIGH);   // Accendi il LED
  tone(speakerPin, 1500);       // Suona un tono a 1500 Hz
  delay(5000);                   // Suona per 500 ms

  // Spegni il LED e ferma il suono
  digitalWrite(ledPin, LOW);    // Spegni il LED
  noTone(speakerPin);           // Ferma il suono
  delay(5000);                   // Pausa per 500 ms
}
