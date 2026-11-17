#include <Arduino.h>
#include <bms_can.h>
#include <bms_parser.h>

// Datos para el frame enviado hacia al bms en el setup().
can_frame rx;
can_frame tx;
canid_t id = 0;
__u8 dlc = 8;
__u8 datos[8] = {0,0,0,0,0,0,0,0};

BmsData bms;

unsigned long timer = 0;

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

// Funcion de parseo pasada como parametros de la funcion que recibe el batch.
void parser(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    bms_parse_can(id, (*ptr_msg).data, bms);
}

// Setup
void setup(){
    Serial.begin(115200);
    while (!Serial){
        ;
    }
    bmsCanInit(modoCan);
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;

// Loop
void loop(){
    if (Serial.available() > 0){
        char tecla = Serial.read();

        switch (tecla) {
            case 's':
                Serial.println("Se manda a domrir al BMS");
                id = 0x80008201;
                datos[7] = 0x55;
                datos[0] = 0x55;
                bmsSend(id, datos, dlc, &tx);
                break;
            case 'a':
                Serial.println("Se despierta al BMS");
                id = 0x80008201;
                datos[7] = 0xAA;
                datos[0] = 0xAA;
                bmsSend(id, datos, dlc, &tx);
                break;
            case 'd':
                Serial.println("Se le pide datos al BMS");
                id = 0x80004200;
                datos[0] = 0;
                datos[7] = 0;
                bmsSend(id, datos, dlc, &tx);
                break;
            case '\r':
            case '\n':
                break;
            default:
                break;

        }
    }
    if (millis() - timer > 5000){
        timer = millis();
        bmsReceiveBatch(&rx, num_bms_frames, batch_timeout, parser);
        Serial.println(bms.sleep);
        Serial.println(bms.balance_charge_request);
    }
}