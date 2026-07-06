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

//can_frame canMsgTx;
can_frame canMsgRx;
bool flag_recibido = false;
bool listening = true;
uint8_t num_frames = 9;
uint8_t contador_frames = 0;

unsigned long tiempo_anterior = 0;


void loop() {
    //limpiarBuffers_Rx();
    flag_recibido = bmsReceive(&canMsgRx, listening);
    if (flag_recibido){
        Serial.println(canMsgRx.can_id, HEX);
        flag_recibido = false;
        contador_frames = contador_frames+1;
    }
    listening = !(contador_frames >= num_frames);

    // Este if lo que hace es volver a escuchar al cabo de 10 segundo. Se puede sacar y para volver a escuchar se hace reset.
    if (millis() - tiempo_anterior > 7000){
        Serial.println("check");
        for (int i=0; i<num_frames; i++){
            Serial.println(canMsgRx.data[i], HEX);
        }
        listening = true;
        contador_frames = 0;
        tiempo_anterior = millis();
        limpiarBuffers_Rx(); 
    }
}