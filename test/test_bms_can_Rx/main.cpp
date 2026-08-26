// mainMCP2515.cpp
#include <Arduino.h>
#include <SPI.h>       // Infraestructura global de bus compartida
#include "bms_can.h"   // Tu módulo de CAN corregido

ModoFuncionamiento modo = MODO_NORMAL;

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
    // 3. Inicializamos tu módulo CAN (configura pines, ISR y Modo Loopback)
    bmsCanInit(modo);
    
    Serial.println("[MAIN] Setup finalizado con éxito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");
}

can_frame canMsgRx;

void imprimirFrame(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    Serial.println(id, HEX);
}

void loop() {
    bmsReceiveBatchBlocking(&canMsgRx, 9, 1500, imprimirFrame);
}