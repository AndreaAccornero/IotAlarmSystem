// Definisci i pin per lo speaker e il LED
const int speakerPin = 25;  // Pin GPIO collegato allo speaker
const int ledPin = 2;       // Pin GPIO collegato al LED

// Definizione delle note musicali (frequenze in Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523

// Melodia (sequenza di note)
int melody[] = {
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4, // Twinkle Twinkle Little Star
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4
};

// Durata delle note (in millisecondi)
int noteDurations[] = {
  500, 500, 500, 500, 500, 500, 1000,   // Durata delle note per ogni nota
  500, 500, 500, 500, 500, 500, 1000
};

void setup() {
  // Inizializza i pin come output
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // Riproduci la melodia
  for (int i = 0; i < 14; i++) {
    int noteDuration = noteDurations[i];

    // Accendi il LED mentre suona
    digitalWrite(ledPin, HIGH);

    // Suona la nota corrente sullo speaker
    tone(speakerPin, melody[i], noteDuration);

    // Pausa tra una nota e l'altra (aggiungi un piccolo ritardo)
    delay(noteDuration * 1.30);  // 30% di pausa tra una nota e l'altra

    // Spegni il LED dopo la nota
    digitalWrite(ledPin, LOW);
    
    // Ferma il suono per un momento
    noTone(speakerPin);
    delay(100);  // Pausa di 100 ms prima di suonare la prossima nota
  }

  // Pausa prima di ripetere la melodia
  delay(2000);  // Pausa di 2 secondi tra le ripetizioni della melodia
}
