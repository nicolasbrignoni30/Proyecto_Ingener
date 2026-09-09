#include <Arduino.h>
#include <SPI.h>
#include <string>


#include "inverter.h"
#include "config.h"

int16_t buffer[1];

#define REG_BMS_TIMEOUT 324
#define REG_EMS_TIMEOUT 331
#define REG_EMS_SHUTDOWN_STANDBY 339
#define REG_HEARTBEAT 8907

void setup(){
    Serial.begin(115200);
    while (!Serial);

    // Se inicializa el inversor y el puerto Uart
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    inverterInit(INVERTER_SERIAL, INVERTER_DE_RE_PIN);
}

void loop() {
    delay(5000);
    inverterRead(REG_BMS_TIMEOUT, 1, buffer);
    Serial.print("El valor del timeout para el bms es: ");
    Serial.println(buffer[0]);


    inverterRead(REG_EMS_TIMEOUT, 1, buffer);
    Serial.print("El valor del timeout para el ems es: ");
    Serial.println(buffer[0]);

    inverterRead(REG_EMS_SHUTDOWN_STANDBY, 1, buffer);
    Serial.print("El valor del ems stdby es: ");
    Serial.println(buffer[0]);

    inverterRead(REG_HEARTBEAT, 1, buffer);
    Serial.print("El valor del registro heartbeat del bms es: ");
    Serial.println(buffer[0]);
};