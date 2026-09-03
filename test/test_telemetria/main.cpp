// Test_telemetria_bms
//
// La idea de este modulo es probar si los atributos compartidos mandados desde 
// thingsboard llegan bien y le logran cambiar su valor
//
// Si bien se incluye al inversor, como a la hora de hacer el test no se lo puede
// probar, no se hace un poll para mandar sus datos debido a que el timeout de
// modbus jode todo.
//
// Con el bms es distinto pues se simula que llegan usando el vector canalyzer.
//
// TEST: Se puede cambiar el valor del atributo 'listen_bms_ms' con el slider en 
// thingsboard y ver como cambia el intervalo de publicacion.
//
// TEST: Se puede cambiar el valor de un registo del inversor (atributo compartido)
// y ver en la terminal serie como la funcion 'inverter_process_pending_writes'
// intenta escribir los nuevos valores.
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <string>

#include "bms_can.h"
#include "bms_parser.h"
#include "inverter.h"
#include "telemetria.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

struct Intervals {
    uint32_t listen_bms_ms;
    uint32_t poll_modbus_ms;
    uint32_t verify_init_ms;
};

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame canMsgRx;
BmsData   bms;
InvData   datosInv;

unsigned long lastPollInv = 0;
unsigned long lastVerifyInv = 0;
unsigned long LastBmsListen = 0;
unsigned long lastPollGasAlarm = 0;

Intervals intervals;

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
    intervals.poll_modbus_ms = DEFAULT_POLL_MODBUS_MS;
    intervals.verify_init_ms = DEFAULT_VERIFY_INIT_MS;
}

void init(){
    SPI.begin();
    // Se inicializan las uart independientes para el inversor y la alarma.
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    GAS_SERIAL.begin(GAS_BAUD, SERIAL_8N1, GAS_RX_PIN, GAS_TX_PIN);

    inverterInit(INVERTER_SERIAL, INVERTER_DE_RE_PIN);
    bmsCanInit(modoCan);
    connectWiFi();
    connectMQTT();
}

void init_all_defaults(){
    inverter_init_defaults();
    init_intervals();
};

// ---------------------------------------------------------------------------
// finalizan las Funciones de Inicializacion
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Funcion de telemetria auxiliar para el inversor
// ---------------------------------------------------------------------------

void publicarTelemetriaInv() {
    publishTelemetryInv(datosInv, "StatusData");
    publishTelemetryInv(datosInv, "AcData");
    publishTelemetryInv(datosInv, "DcData");
    publishTelemetryInv(datosInv, "GridData");
    publishTelemetryInv(datosInv, "LoadData");
}


void intervals_update(const String& key, float value) {
    if      (key == "listen_bms_ms")  intervals.listen_bms_ms  = (uint32_t)value;
    else if (key == "poll_modbus_ms") intervals.poll_modbus_ms = (uint32_t)value;
    else if (key == "verify_init_ms") intervals.verify_init_ms = (uint32_t)value;
    else return;
}

// Las funciones a continuacion se definen aca pero son las que se pasan en los callbacks definidos en el modulo de telemetria.

void telemetria_set_attribute_handler1(const String& key, float value) {
    inverter_update_reg_values(key, value);
    intervals_update(key, value);
}

void telemetria_set_attribute_handler2(const String& key, float value) {
    inverter_queue_write(key, value);
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

    // A diferencia de con thermal_control, para el inversor hay que volver a escribir los registros
    inverter_reinit_from_cloud();

    // Se modifica el Callback para adecuarlo a cuando cambian algunos atributos.
    setCallback();
    suscribe_attributes();
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
    // Se chequean tanto la conexion wifi como mqtt
    if (!checkWiFiConnection()) connectWiFi();
    if (!checkMQTTConnection()) {connectMQTT(); setCallback(); suscribe_attributes();}

    // Se llama periodicamente a loopMQTT para llamar al callback si atributos cambiaron.
    loopMQTT();

    // Se envian hacia el inversor los registros que cambiaron y guardados en la cola
    inverter_process_pending_writes();

    // Se reciben los 9 frames que envia el bms se parsea lo importante y se envia a thingsboard
    if (millis() - LastBmsListen > intervals.listen_bms_ms){
        LastBmsListen = millis();
        bmsReceiveBatch(&canMsgRx, num_bms_frames, batch_timeout, parser);
        publishTelemetryBMS(bms);
    }
}