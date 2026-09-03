// Test_pasamanos
//
// La idea de este modulo es probar si se logra leer desde el bms y ecribir en
// los registros 6000 en adelante del inversor (en la datasheet figuran como
// registro de solo lectura).
//
// Para esto se escribe y enseguida se leen en el bucle
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <string>

#include "bms_can.h"
#include "bms_parser.h"
#include "inverter.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame canMsgRx;
BmsData   bms;
//InvData   datosInv;

unsigned long Last_time = 0;

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void parser(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    bms_parse_can(id, (*ptr_msg).data, bms);
}

// ---------------------------------------------------------------------------
// Funciones de Inicializacion
// ---------------------------------------------------------------------------

void init(){
    SPI.begin();
    // Se inicializan las uart independientes para el inversor y la alarma.
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    GAS_SERIAL.begin(GAS_BAUD, SERIAL_8N1, GAS_RX_PIN, GAS_TX_PIN);

    inverterInit(INVERTER_SERIAL, INVERTER_DE_RE_PIN);
    bmsCanInit(modoCan);
}

void init_all_defaults(){
    inverter_init_defaults();
};

// ---------------------------------------------------------------------------
// finalizan las Funciones de Inicializacion
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Funcion de Pasamanos, esta se tiene que hacer en el main si o si
// ---------------------------------------------------------------------------

void pasamanos(){
    inverterWrite(BMS_BATTERY_SOC, (int16_t)bms.soc_pct);
    inverterWrite(BMS_BATTERY_SOH, (int16_t)bms.soh_pct);
    inverterWrite(BMS_MAX_CHG_CURRENT,    (int16_t)(bms.max_charge_a    / SCALE_CURRENT_A));
    inverterWrite(BMS_MAX_DISCHG_CURRENT, (int16_t)(bms.max_discharge_a / SCALE_CURRENT_A));
}


// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    while (!Serial);

    // Se inicializan todos lo modulo.
    init();

    // Se incializan todos los valores default puesto en la memoria flash.
    init_all_defaults();
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;
int16_t buffer[4];

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // Se reciben los 9 frames que envia el bms se parsea lo importante y se envia a thingsboard
    if (millis() - Last_time > 5000){
        Last_time = millis();
        bmsReceiveBatch(&canMsgRx, num_bms_frames, batch_timeout, parser);
        pasamanos();
        inverterRead(BMS_BATTERY_SOC, 4, buffer);
        Serial.println(buffer[0]);
        Serial.println(buffer[1]);
        Serial.println((float)(buffer[2] * SCALE_CURRENT_A));
        Serial.println((float)(buffer[3] * SCALE_CURRENT_A));
    }
}