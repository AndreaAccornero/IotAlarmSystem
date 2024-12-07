import paho.mqtt.client as mqtt
import json

mqtt_broker = "localhost"
mqtt_port = 1883
mqqt_topic = "iot/bed_alarm"
mqtt_topic_sampling_rate = "iot/bed_alarm/update_sampling_rate"
mqtt_topic_stop_alarm = "iot/bed_alarm/stop_alarm"

sampling_rate = 3500
stop_alarm = True

# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT broker with result code {rc}")
    client.subscribe(mqqt_topic)


def on_message(client, userdata, msg):
    print(f"Messaggio ricevuto su topic {msg.topic}: {msg.payload.decode()}")
    # global sampling_rate
    # payload = msg.payload.decode('utf-8')
    # print(f"Received message '{payload}' on topic '{msg.topic}'")
    # try:
    #     command = json.loads(payload)
    #     if 'sampling_rate' in command:
    #         sampling_rate = int(command['sampling_rate'])
    #         print(f"Updated sampling rate to {sampling_rate} seconds")
            
    # except json.JSONDecodeError:
    #     print("Invalid command format")

if __name__ == "__main__":
    # Inizializza il client MQTT
    mqtt_client = mqtt.Client("PythonClient")
    mqtt_client.connect(mqtt_broker, mqtt_port, 60)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.loop_start()

    mqtt_client.publish(mqtt_topic_sampling_rate, json.dumps({"sampling_rate": sampling_rate}))
    mqtt_client.publish(mqtt_topic_stop_alarm, json.dumps({"stop_alarm": stop_alarm}))
    mqtt_client.loop_stop()