// mainMCP2515.cpp
#include <Arduino.h>
#include <SPI.h>       // Infraestructura global de bus compartida
#include "bms_can.h"   // Tu módulo de CAN corregido

// Variables necesarias para crear el paquete
can_frame frame;
can_frame canMsgRx;

// Variables realtivas a leer un comando desde la terminal serie
canid_t tecladoID = 0;
__u8 tecladoDatos[8] = {0};
__u8 tecladoLargo = 0;

// Se define el modo de funcionamento, en este caso Loopback
ModoFuncionamiento modo = MODO_LOOPBACK;

void setup() {
    //  Inicializamos el monitor serie a alta velocidad
    Serial.begin(115200);
    while(!Serial); // Espera a que se abra la terminal (útil en algunas placas)
    
    Serial.println("\n==================================================");
    Serial.println("[MAIN] Iniciando Banco de Pruebas MCP2515...");
    Serial.println("==================================================");

    // Encendemos el motor del bus SPI global de la placa
    SPI.begin(); 
    Serial.println("[MAIN] Bus SPI global inicializado.");
    
    // 3. Inicializamos tu módulo CAN (configura pines, ISR y Modo Loopback)
    bmsCanInit(modo);
    Serial.println("[MAIN] MCP2515 inicializado.");
    
    Serial.println("[MAIN] Setup finalizado con éxito. Corriendo lazo...");
    Serial.println("--------------------------------------------------");


    Serial.println("\n==================================================");
    Serial.println("[MAIN] Consola Interactiva CAN - MCP2515");
    Serial.println("==================================================");
    Serial.println("Instrucciones: Escribí en la terminal el mensaje a enviar.");
    Serial.println("Formato (en HEX): ID:Byte0,Byte1,Byte2...");
    Serial.println("Ejemplo: 355:5F,00,64,00  (ID 0x355 con 4 bytes)");
    Serial.println("--------------------------------------------------");
}

bool listening = true;
bool flag_recibido = false;
//unsigned long tiempo_anterior = 0;
byte sndStat = 0;

canid_t id = 0x4210;
__u8 dlc = 8;
__u8 datos[8] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

void loop() {
    flag_recibido = bmsReceive(&canMsgRx, listening);
    if (flag_recibido){
        imprimirRx(&canMsgRx);
        //asignarPaquete(&canMsgRx);
        //imprimir_0x421();
        flag_recibido = false;
        Serial.println("\n==================================================");
        Serial.println("[MAIN] Consola Interactiva CAN - MCP2515");
        Serial.println("==================================================");
        Serial.println("Instrucciones: Escribí en la terminal el mensaje a enviar.");
        Serial.println("Formato (en HEX): ID:Byte0,Byte1,Byte2...");
        Serial.println("Ejemplo: 355:5F,00,64,00  (ID 0x355 con 4 bytes)");
        Serial.println("--------------------------------------------------");
    }

    //if (millis() - tiempo_anterior > 100){ //5 segundos
        //mkMsg(id, datos, dlc, &frame);
        //Serial.println(frame.can_id, HEX);
        //byte sndStat = bmsSend(&frame);
        //tiempo_anterior = millis(); 
    //}
    // El codigo anterior es por si si quiere excribir el mensaje a enviar en la terminal serie.
    // ID:Byte0,Byte1,Byte2...

    if (Serial.available() > 0) {
        //Leemos la línea entera hasta que se presione Enter (\n)
        String entrada = Serial.readStringUntil('\n');
    

        // Si el formato es correcto, procesamos y enviamos
        if (procesarEntradaTeclado(entrada, tecladoID, tecladoDatos, tecladoLargo)) {
            Serial.println("\n--------------------------------------------------");
            Serial.printf("[TECLADO] Detectado ID: 0x%X | Cantidad de Bytes: %d\n", tecladoID, tecladoLargo);
            
            // Usamos tu fábrica universal con los datos dinámicos del teclado
            mkMsg(tecladoID, tecladoDatos, tecladoLargo, &frame);
            
            //Despachamos el paquete al chip
            Serial.println("[CAN] Enviando paquete interactivo...");
            byte sndStat = bmsSend(&frame);
            //Serial.print("[DEBUG] Estado del envío: ");
            //Serial.println(sndStat);
            Serial.println("--------------------------------------------------");
        }
    }
}