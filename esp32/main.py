import network
import urequests
import umqtt.simple
import ujson
import time
from machine import Pin, ADC, PWM
# from config import SSID, PASSWORD

SSID = "iliadbox-69399A"
PASSWORD = "rb6zxqdv2sqdm52vqb62nz"

# Configurazione dei pin
PRESSURE_SENSOR_PIN = 33  # Pin collegato al sensore di pressione
LED_BUILTIN = 2           # Pin del LED integrato
SPEAKER_PIN = 32          # Pin dello speaker
PRESSURE_THRESHOLD = 4095 # Soglia per attivare LED e speaker

# Configurazione HTTP
SERVER_URL = "http://192.168.1.124:5000/sensor_data"

# Configurazione MQTT
MQTT_BROKER = "192.168.1.124"
MQTT_PORT = 1883
MQTT_TOPIC = "iot/bed_alarm/sampling_rate"

# Inizializzazione dei pin
sensor = ADC(Pin(PRESSURE_SENSOR_PIN))
sensor.atten(ADC.ATTN_11DB)  # Imposta l'attenuazione per leggere l'intero range 0-3.3V
led = Pin(LED_BUILTIN, Pin.OUT)
speaker = PWM(Pin(SPEAKER_PIN), freq=2000, duty=0)

# Variabili globali
sampling_rate = 5000  # Intervallo tra le letture (in millisecondi)
last_sample_time = 0

# Connessione WiFi
def connect_to_wifi():
    print("Connecting to WiFi...")
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(SSID, PASSWORD)

    while not wlan.isconnected():
        time.sleep(1)
        print(".")

    print("\nConnected to WiFi. IP:", wlan.ifconfig()[0])

# Connessione MQTT
def connect_to_mqtt():
    global mqtt_client
    mqtt_client = umqtt.simple.MQTTClient("ESP32Client", MQTT_BROKER, MQTT_PORT)
    mqtt_client.set_callback(mqtt_callback)
    
    while True:
        try:
            mqtt_client.connect()
            print("Connected to MQTT")
            mqtt_client.subscribe(MQTT_TOPIC)
            break
        except Exception as e:
            print("MQTT connection failed:", str(e))
            time.sleep(5)

# Callback per MQTT
def mqtt_callback(topic, msg):
    global sampling_rate
    print("Received MQTT message on topic:", topic.decode(), "->", msg.decode())

    try:
        data = ujson.loads(msg.decode())
        if "sampling_rate" in data:
            sampling_rate = int(data["sampling_rate"]) * 1000
            print("Updated sampling_rate:", sampling_rate)
    except Exception as e:
        print("Error parsing JSON:", str(e))

# Invio dati al server Flask
def send_pressure_data(pressure_value):
    try:
        response = urequests.post(SERVER_URL, json={"pressure_value": pressure_value})
        print("Data sent successfully. Response:", response.text)
    except Exception as e:
        print("Error sending data:", str(e))

# Gestione dell'allarme (LED + speaker)
def handle_alert(pressure_value):
    if pressure_value >= PRESSURE_THRESHOLD:
        led.value(1)  # Accendi LED
        speaker.duty(512)  # Attiva suono
    else:
        led.value(0)  # Spegni LED
        speaker.duty(0)  # Ferma suono

# Avvio del sistema
connect_to_wifi()
connect_to_mqtt()

while True:
    current_time = time.ticks_ms()

    if time.ticks_diff(current_time, last_sample_time) >= sampling_rate:
        last_sample_time = current_time

        # Lettura sensore
        pressure_value = sensor.read()
        print("Valore del sensore:", pressure_value)

        # Gestione allarme
        handle_alert(pressure_value)

        # Invio dati al server
        send_pressure_data(pressure_value)

    mqtt_client.check_msg()  # Controlla nuovi messaggi MQTT
    time.sleep(0.1)  # Piccola pausa per non sovraccaricare la CPU
