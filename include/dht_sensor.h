#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include <Arduino.h>

// Estructura con los datos leidos del sensor
struct DhtData {
    float temperature_c;   // Temperatura en grados Celsius
    float humidity_pct;    // Humedad relativa en %
    bool valid;            // true si la ultima lectura fue exitosa
};

// Inicializa el sensor DHT22 en el pin indicado.
// Debe llamarse una vez en setup() antes de usar dhtRead().
void dhtInit(uint8_t pin);

// Lee temperatura y humedad del sensor.
// Devuelve una struct DhtData; si la lectura falla (sensor
// desconectado, timing incorrecto, etc.) valid = false y
// temperature_c / humidity_pct quedan en NAN.
DhtData dhtRead();

#endif // DHT_SENSOR_H