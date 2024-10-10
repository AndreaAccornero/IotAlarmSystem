import paho.mqtt.client as mqtt

# Configura il broker MQTT
broker = "localhost"  # Broker Mosquitto su localhost
port = 1883
topic = "test"
# Funzione callback per quando il client si connette al broker
def on_connect(client, userdata, flags, rc):
    print("Connesso al broker MQTT con codice di ritorno " + str(rc))
    client.subscribe(topic)  # Sottoscrivi al topic

# Funzione callback per quando si riceve un messaggio
def on_message(client, userdata, msg):
    print(f"Messaggio ricevuto su topic {msg.topic}: {msg.payload.decode()}")

# Crea il client MQTT
client = mqtt.Client("PythonClient")
client.on_connect = on_connect
client.on_message = on_message

# Connessione al broker
client.connect(broker, port, 60)

# Avvia il loop per gestire la comunicazione MQTT
client.loop_start()

# Pubblica un messaggio ogni 5 secondi per testare
import time
while True:
    client.publish(topic, "Messaggio di test da Python!")
    time.sleep(5)
