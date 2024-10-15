// Definizione dei pin
const int speakerPin = 32;  // Pin GPIO collegato allo speaker
const int fsrPin = 35;    // Pin analogico collegato al FSR 
const int ledPin = 2;     // Pin collegato al LED integrato 

// Variabile per memorizzare la lettura del FSR
int fsrValue = 0;

// Soglia di pressione per accendere il LED
int threshold = 1;  // Cambia questo valore in base alla sensibilità che desideri

void setup() {
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Inizializza la comunicazione seriale per il debug
  Serial.begin(115200);

  // Imposta il pin analogico come input (opzionale, è già implicitamente un input)
  pinMode(fsrPin, INPUT);
}

void loop() {
  // Legge il valore analogico dal FSR
  fsrValue = analogRead(fsrPin);

  // Stampa il valore del sensore per il debug
  Serial.print("FSR Value: ");
  Serial.println(fsrValue);

  // Se il valore supera la soglia, accendi il LED
  if (fsrValue > threshold) {
    digitalWrite(ledPin, LOW);   // Spegni il LED
    noTone(speakerPin);  
    
  } else {
    digitalWrite(ledPin, HIGH);  // Accendi il LED
    tone(speakerPin, 1000);   
  }

  // Ritardo per evitare letture troppo frequenti
  delay(100);
}
