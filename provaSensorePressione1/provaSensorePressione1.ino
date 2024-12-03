#include <WiFi.h>
#include <HTTPClient.h>


// Definizione dei pin
const int speakerPin = 32;  // Pin GPIO collegato allo speaker
const int fsrPin = 35;    // Pin analogico collegato al FSR 
const int ledPin = 2;     // Pin collegato al LED integrato 

// Configurazione WiFi
#include <../credentials.h>/ // Credenziali WiFi
const char* serverName = "http://192.168.1.124:5000/sensor_data";  

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

  // Connetti alla rete WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
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

  // Invia i dati al server Flask
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"pressure_value\": " + String(fsrValue) + "}";
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }

  // Ritardo per evitare letture troppo frequenti
  delay(sampling_rate);
}
