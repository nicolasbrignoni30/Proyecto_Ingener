#include "dht_sensor.h"
#include "config.h"

void setup() {
    Serial.begin(115200);
    dhtInit(DHT_PIN);
}

void loop() {
    DhtData d = dhtRead();

    if (d.valid) {
        Serial.printf("Temp: %.2f C  Hum: %.2f %%\n", d.temperature_c, d.humidity_pct);
    }

    delay(2000); // el DHT22 no soporta lecturas más rápido que ~2s
}