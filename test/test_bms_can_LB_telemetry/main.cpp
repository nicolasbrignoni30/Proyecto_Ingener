// mainMCP2515.cpp
#include <Arduino.h>
#include <SPI.h> 
#include "bms_can.h"  
#include "bms_parser.h"
#include "telemetria.h"


// Variables necesarias para crear el paquete
can_frame frame;
can_frame canMsgRx;

byte sndStat = 0;

BmsData bms;

// Se define el modo de funcionamento, en este caso Loopback
ModoFuncionamiento modo = MODO_LOOPBACK;

void setup() {
    //  Inicializamos el monitor serie a alta velocidad
    Serial.begin(115200);
    while(!Serial); // Espera a que se abra la terminal (útil en algunas placa)

    // Encendemos el motor del bus SPI global de la placa
    SPI.begin(); 
    
    // 3. Inicializamos tu módulo CAN (configura pines, ISR y Modo Loopback)
    bmsCanInit(modo);

    // 4. Inicializamos wifi y mqtt.
    connectWiFi();
    connectMQTT();
}

bool listening = true;
bool flag_recibido = false;
unsigned long lastPublishBms = 0;
unsigned long lastListenBms = 0;
uint8_t contador_frames = 0;
const uint8_t num_frames = 6;

// Variables realtivas a leer un comando desde la terminal serie
canid_t tecladoID = 0;
__u8 tecladoDatos[8] = {0};
__u8 tecladoLargo = 0;

void loop() {
    flag_recibido = bmsReceive(&canMsgRx, listening);
    if (flag_recibido){
        imprimirRx(&canMsgRx);
        bms_parse_can(canMsgRx.can_id, canMsgRx.data, bms);
        flag_recibido = false;
        contador_frames = contador_frames + 1;
    }

    listening = !(contador_frames >= num_frames);

    if (millis() - lastListenBms > LISTEN_INTERVAL_BMS){
        listening = true;
        contador_frames = 0;
    }

    if (millis() - lastPublishBms > PUBLISH_INTERVAL_BMS){
        publishTelemetryBMS(bms);
        lastPublishBms = millis();
    }

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
            sndStat = bmsSend(&frame);
            //Serial.print("[DEBUG] Estado del envío: ");
            //Serial.println(sndStat);
            Serial.println("--------------------------------------------------");
        }
    }    
}

// Valores para probar en la terminal serie
//  El orden es: byte 0 / byte 1 / ... / byte 8

//4210:00,00,00,00,00,00,00,00
//4220:00,00,00,00,00,00,00,00
//4230:00,00,00,00,00,00,00,00
//4240:00,00,00,00,00,00,00,00
//4250:00,00,00,00,00,00,00,00
//4280:00,00,00,00,00,00,00,00

//4210:AA,AA,AA,AA,AA,AA,AA,AA
//4220:BB,BB,BB,BB,BB,BB,BB,BB
//...