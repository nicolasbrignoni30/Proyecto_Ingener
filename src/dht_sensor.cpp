#include "dht_sensor.h"
#include <DHT.h>

static DHT* ptr_dht = nullptr;

void dhtInit(uint8_t pin) {
    // DHT22 = AM2302
    ptr_dht = new DHT(pin, DHT22);
    ptr_dht->begin();
}

DhtData dhtRead() {
    DhtData datos;

    if (ptr_dht == nullptr) {
        // dhtInit() no fue llamado
        datos.valid = false;
        datos.temperature_c = NAN;
        datos.humidity_pct = NAN;
        return datos;
    }

    float temp = ptr_dht->readTemperature();
    float hum  = ptr_dht->readHumidity();

    // La libreria devuelve NAN si la lectura falla
    if (isnan(temp) || isnan(hum)) {
        datos.valid = false;
        datos.temperature_c = NAN;
        datos.humidity_pct = NAN;
        Serial.println("[DHT] Error de lectura del sensor");
        return datos;
    }

    datos.valid = true;
    datos.temperature_c = temp;
    datos.humidity_pct = hum;
    return datos;
}