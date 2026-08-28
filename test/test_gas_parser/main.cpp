#include <Arduino.h>
#include "gas_parser.h"

uint16_t sim_reg[] = {0x41C8, 0x0000,
                      0x4248, 0x0000,
                      0x42C8, 0x0000,
                      0x3F80, 0x0000,
                      0x4000, 0x0000,
                      0x4130, 0x0000,
                      0x0000, 0x0000,
                      0x0000, 0x0000};

GasData gas;

void setup(){
    Serial.begin(115200);
    delay(1000);


    gas_parse_all(sim_reg, gas);
    Serial.println(gas.alarm1_point);
    Serial.println(gas.alarm2_point);
    Serial.println(gas.range);
    Serial.println(gas.resolution);
    Serial.println(gas.unit);
    Serial.println(gas.gas_type);
    Serial.println(gas.concentration);
    Serial.println(gas.alarm_status); 
};

void loop() {}