// mainMCP2515.cpp
#include <Arduino.h>
#include <SPI.h>       // Infraestructura global de bus compartida
#include "bms_can.h"   // Tu módulo de CAN corregido

// Variables necesarias para crear el paquete
canid_t id = 0x07;
__u8 dlc = 8;
__u8 datos[8] = {0xAA, 0xBB, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
can_frame frame;
ModoFuncionamiento modo = MODO_NORMAL;

// Variables realtivas a leer un comando desde la terminal serie
//canid_t tecladoID = 0;
//__u8 tecladoDatos[8] = {0};
//__u8 tecladoLargo = 0;

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
    Serial.println("[MAIN] MCP2515 inicializado.");
    
    Serial.println("[MAIN] Setup finalizado con éxito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");

    // Se arma el paquete antes de entrar en el loop
    mkMsg(id, datos, dlc, &frame);
}


void loop() {
    byte sndStat = bmsSend(&frame);
    
    Serial.print("[DEBUG] Estado del envío: ");
    Serial.println(sndStat);

    delay(1000);

    // El codigo anterior es por si si quiere excribir el mensaje a enviar en la terminal serie.
    // ID:Byte0,Byte1,Byte2...

    //if (Serial.available() > 0) {
        // Leemos la línea entera hasta que se presione Enter (\n)
        //String entrada = Serial.readStringUntil('\n');
        // Si el formato es correcto, procesamos y enviamos
        //if (procesarEntradaTeclado(entrada, tecladoID, tecladoDatos, tecladoLargo)) {
            //Serial.println("\n--------------------------------------------------");
            //Serial.printf("[TECLADO] Detectado ID: 0x%X | Cantidad de Bytes: %d\n", tecladoID, tecladoLargo);
            // Usamos tu fábrica universal con los datos dinámicos del teclado
            //mkMsg(tecladoID, tecladoDatos, tecladoLargo, &canMsgTx);
            //Despachamos el paquete al chip
            //Serial.println("[CAN] Enviando paquete interactivo...");
            //bmsSend(&canMsgTx); 
            //Serial.println("--------------------------------------------------");
        //}
    //}
}