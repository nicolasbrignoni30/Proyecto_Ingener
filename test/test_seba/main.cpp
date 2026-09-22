#include <Arduino.h>
#include <SPI.h>
#include <string>

#include "bms_can.h"
#include "bms_parser.h"
#include "telemetria.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

struct Intervals {
    uint32_t listen_bms_ms;
};

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame canMsgRx;
BmsData   bms;

unsigned long LastBmsListen = 0;

Intervals intervals;

// Datos para el frame enviado hacia al bms en el setup().
can_frame tx;
canid_t id = 0x80004200;
__u8 dlc = 8;
__u8 datos[8] = {0,0,0,0,0,0,0,0};


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

void init_intervals() {
    intervals.listen_bms_ms  = DEFAULT_LISTEN_BMS_MS;
}

void init(){
    SPI.begin();
    // Se inicializan las uart independientes para el inversor y la alarma.
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    
    bmsCanInit(modoCan);
    connectWiFi();
    connectMQTT(Callback_setup);
}

void init_all_defaults(){
    init_intervals();
};

// ---------------------------------------------------------------------------
// finalizan las Funciones de Inicializacion
// ---------------------------------------------------------------------------



void intervals_update(const String& key, float value) {
    if      (key == "listen_bms_ms")  intervals.listen_bms_ms  = (uint32_t)value;
    else return;
}

// Las funciones a continuacion se definen aca pero son las que se pasan en los callbacks definidos en el modulo de telemetria.

void telemetria_set_attribute_handler1(const String& key, float value) {
    intervals_update(key, value);
}

void telemetria_set_attribute_handler2(const String& key, float value) {
    intervals_update(key, value);
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

    // Se piden los atributos 
    request_attributes();
    delay(8000); // Este pequeño delay es para que lleguen bien las cosas

    // Se llama a loopMQTT()
    loopMQTT();
    
    // Se modifica el Callback para adecuarlo a cuando cambian algunos atributos.
    setCallback();
    suscribe_attributes();

    // Se envia el frame que hace que el bms comience a mandar periodicamente sus paquetes
    bmsSend(id, datos, dlc, &tx);
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;
int16_t buffer[9];

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // Se chequean tanto la conexion wifi como mqtt
    if (!checkWiFiConnection()) connectWiFi();
    if (!checkMQTTConnection()) {
        Serial.print("MQTT desconectado, state: ");
        mqttstate();
        connectMQTT(Callback_loop);
        setCallback();
        suscribe_attributes();
    }

    // Se llama periodicamente a loopMQTT para llamar al callback si atributos cambiaron.
    loopMQTT();


    // Se reciben los 9 frames que envia el bms se parsea lo importante y se envia a thingsboard
    if (millis() - LastBmsListen > intervals.listen_bms_ms){
        LastBmsListen = millis();
        bmsReceiveBatch(&canMsgRx, num_bms_frames, batch_timeout, parser);
        publishTelemetryBMS(bms);
    }
}