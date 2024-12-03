from influxdb_client import InfluxDBClient, Point, WriteOptions
import time
from dotenv import load_dotenv 
import os

load_dotenv(".env")

# Configurazione di InfluxDB
token = os.getenv("token")
org = "IoTAlarmSystem"
bucket = "Prova"

client = InfluxDBClient(url="http://localhost:8086", token=token, org=org)

write_api = client.write_api(write_options=WriteOptions(batch_size=1))

# Funzione per generare dati di esempio
def generate_data():
    # Puoi modificare questa funzione per generare dati realistici
    return [
        {"measurement": "temperature", "tags": {"location": "office"}, "fields": {"value": 23.5}},
        {"measurement": "humidity", "tags": {"location": "office"}, "fields": {"value": 40}},
    ]

# Numero di dati da scrivere (esempio: 10 misurazioni)
num_measurements = 10

for i in range(num_measurements):
    data = generate_data()
    for point in data:
        try:
            write_api.write(bucket=bucket, record=Point.from_dict(point))
            print(f"Dato scritto: {point}")
        except Exception as e:
            print(f"Errore durante la scrittura dei dati: {e}")
    
    # Aspetta 5 secondi prima di scrivere la prossima misurazione
    time.sleep(5)

# Aspetta qualche secondo per completare tutte le operazioni
time.sleep(2)

# Chiudi la connessione a InfluxDB
client.close()
 
