#include <WiFi.h>
#include <PubSubClient.h> // Libreria MQTT

// Definizione dei pin per lo speaker e il LED
const int speakerPin = 25;  // Pin GPIO collegato allo speaker
const int ledPin = 2;       // Pin GPIO collegato al LED

// Credenziali WiFi
const char* ssid = "iliadbox-69399A";          // Modifica con il nome della tua rete WiFi
const char* password = "rb6zxqdv2sqdm52vqb62nz";       // Modifica con la password della tua rete WiFi

// Indirizzo del broker MQTT (es. l'IP del tuo PC su cui è attivo Mosquitto)
// 192.168.1.124
const char* mqtt_server = "192.168.1.115";     // Modifica con l'IP corretto del broker MQTT

WiFiClient espClient;
PubSubClient client(espClient);

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
}