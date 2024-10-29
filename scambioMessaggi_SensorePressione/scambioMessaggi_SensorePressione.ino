#include <WiFi.h>
#include <PubSubClient.h> // Libreria MQTT
#include <../credentials.h>/ // Credenziali WiFi

// Definizione dei pin
const int speakerPin = 25;  // Pin GPIO collegato allo speaker
const int fsrPin = 35;    // Pin analogico collegato al FSR
const int ledPin = 2;     // Pin collegato al LED integrato

// Indirizzo del broker MQTT (es. l'IP del tuo PC su cui è attivo Mosquitto)
const char* mqtt_server = "192.168.1.124";     // Modifica con l'IP corretto del broker MQTT

WiFiClient espClient;
PubSubClient client(espClient);

// Variabile per memorizzare la lettura del FSR
int fsrValue = 0;

// Soglia di pressione per accendere il LED
int threshold = 1;  // Cambia questo valore in base alla sensibilità che desideri

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
  NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4, NOTE_A4, NOTE_A4, NOTE_G4,
  NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4, NOTE_D4, NOTE_D4, NOTE_C4
};

// Durata delle note (in millisecondi)
int noteDurations[] = {
  500, 500, 500, 500, 500, 500, 1000,
  500, 500, 500, 500, 500, 500, 1000
};

// Funzione per connettersi al WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connessione a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connesso");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());
}

// Funzione di callback per ricevere messaggi MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Messaggio ricevuto su topic: ");
  Serial.print(topic);
  Serial.print(". Messaggio: ");
  String message;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  // Se il messaggio è "play_melody", riproduci la melodia
  if (message == "play_melody") {
    playMelody();
  }
}

// Funzione per connettersi al broker MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connessione al broker MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("connesso");
      client.subscribe("esp32/commands"); // Modifica con il topic desiderato
    } else {
      Serial.print("fallito, rc=");
      Serial.print(client.state());
      Serial.println(" riprovo tra 5 secondi");
      delay(5000);
    }
  }
}

// Funzione per riprodurre la melodia
void playMelody() {
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

  // Pausa prima di ripetere la melodia (se necessario)
  delay(2000);  // Pausa di 2 secondi tra le ripetizioni della melodia
}

void setup() {
  // Inizializza i pin come output
  pinMode(speakerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Inizializza la comunicazione seriale
  Serial.begin(115200);

  // Connettiti al WiFi
  setup_wifi();

  // Configura il broker MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Legge il valore analogico dal FSR
  fsrValue = analogRead(fsrPin);

  // Stampa il valore del sensore per il debug
  Serial.print("FSR Value: ");
  Serial.println(fsrValue);

  // Se il valore supera la soglia, spegni il LED e invia un messaggio al broker MQTT
  if (fsrValue > threshold) {
    digitalWrite(ledPin, LOW);   // Spegni il LED
    noTone(speakerPin);
    client.publish("esp32/commands", "pressure_detected");  // Invia il messaggio al broker
  } else {
    digitalWrite(ledPin, HIGH);  // Accendi il LED
    tone(speakerPin, 1000);   // Suona un tono
  }

  // Ritardo per evitare letture troppo frequenti
  delay(100);
}
