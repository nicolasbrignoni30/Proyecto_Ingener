// mainMCP2515.cpp
#include <Arduino.h>
#include <SPI.h>       // Infraestructura global de bus compartida
#include "bms_can.h"   // Tu módulo de CAN corregido
#include "bms_parser.h"
#include "thermal_control.h"
#include "telemetria.h"
#include "dht_sensor.h"
#include "inverter.h"

ModoFuncionamiento modo = MODO_NORMAL;

// Funciones para poder imprimir los estados y las temperaturas
const char* nombreEstado(ThermalState s) {
    switch (s) {
        case THERMAL_STATE_MONITOR: return "MONITOR";
        case THERMAL_STATE_HEATING: return "HEATING";
        case THERMAL_STATE_COOLING: return "COOLING";
    }
    return "?";
}

void imprimirEstado(unsigned long time_ms, float t_amb, const BmsData& bms) {
    Serial.printf(
        "[t=%6lu ms] temp_min=%.1f temp_max=%.1f temp_amb=%.1f -> estado=%s | FAN=%d HEAT=%d\n",
        time_ms, bms.temp_cell_min_c, bms.temp_cell_max_c, t_amb, nombreEstado(thermalControlGetState()),
        digitalRead(FAN_RELAY_PIN), digitalRead(HEATING_RELAY_PIN)
    );
}

void setup() {
    // 1. Inicializamos el monitor serie a alta velocidad
    Serial.begin(115200);
    while(!Serial); // Espera a que se abra la terminal (útil en algunas placas)
    
    Serial.println("\n==================================================");
    Serial.println("[MAIN] Iniciando Banco de Pruebas MCP2515...");
    Serial.println("==================================================");

   
    // 2. Encendemos el motor del bus SPI global de la placa
    SPI.begin(); 
    Serial.println("[MAIN] Bus SPI global inicializado.");

    // 3. Inicializamos los modulos
    bmsCanInit(modo);
    thermalControlInit();
    connectWiFi();
    connectMQTT();
    dhtInit(DHT_PIN);

    Serial.println("[MAIN] Setup finalizado con éxito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");
}

can_frame canMsgRx;
BmsData bms;
DhtData d;
float temp = 30.0;

void parser(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    bms_parse_can(id, (*ptr_msg).data, bms);
}

void sendCoolingState(float T, bool b1, bool b2){
    publishTemperature(T, b1, b2);
}

void loop() {
    bmsReceiveBatchBlocking(&canMsgRx, 9, 5500/*1500*/, parser);
    d = dhtRead();
    if (millis() > 7000){
        temp = 65.0;
    }
    if (thermalControlUpdate(millis(), bms.temp_cell_min_c, bms.temp_cell_max_c, temp/*d.temperature_c*/, sendCoolingState)){
        inverterWrite(REG_SHUTDOWN, 1);
        Serial.println("Efectivamente paso por aca y le pidio al inv que se apague");
    }
    imprimirEstado(millis(), temp/*d.temperature_c*/, bms);
}