// =============================================================================
// main.cpp — Firmware final del banco de baterías
//
// Integra todos los módulos ya probados por separado en test/:
//   - bms_can        : lectura del BMS por CAN (MCP2515)
//   - dht_sensor     : temperatura/humedad ambiente
//   - thermal_control: máquina de estados de ventiladores / heating plates
//   - inverter       : lectura/escritura del inversor por Modbus RTU (RS485)
//   - telemetria     : WiFi + MQTT hacia ThingsBoard
//
// Loop principal:
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <string>

#include "bms_can.h"
#include "bms_parser.h"
#include "inverter.h"
#include "telemetria.h"
#include "thermal_control.h"
#include "dht_sensor.h"
#include "gas_alarm.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

struct Intervals {
    uint32_t listen_bms_ms;
    uint32_t poll_modbus_ms;
    uint32_t verify_init_ms;
    uint32_t poll_gas_alarm_ms;
};

// Funcionamiento del MCP2515
ModoFuncionamiento modoCan = MODO_NORMAL;

can_frame canMsgRx;
BmsData   bms;
DhtData   dht;
InvData   datosInv;
DhtData   T_sensor;
GasData   G_alarm;

unsigned long lastPollInv = 0;
unsigned long lastVerifyInv = 0;
unsigned long LastBmsListen = 0;
unsigned long lastPollGasAlarm = 0;

// Globales, arriba de setup() y loop()
bool          primera_vez        = true;   // fuerza 329/334/351 en la primera vuelta
bool          apagado_solicitado = false;  // true cuando el control térmico manda 651
float         setpoint_red_kw    = 0.0f;   // lo fijará la máquina de estados
unsigned long reg_10_seg         = 0;

Intervals intervals;

// Datos para el frame enviado hacia al bms en el setup().
can_frame tx;
canid_t id = 0x80004200;
__u8 dlc = 8;
__u8 datos[8] = {0,0,0,0,0,0,0,0};


// ---------------------------------------------------------------------------
// Control del inversor
// ---------------------------------------------------------------------------

enum Estado { IDLE, CHARGING_VEHICLE, CHARGING_BATTERY };
Estado STATE = IDLE;

bool grid_ok = false;           // reg 32: b3 = 1 (con red) y b4 = 0 (no isla)
bool inv_ok  = false;           // reg 32: b2 = 1 (encendido) y b0 = 0 (sin falla)
bool apagado_termico = false;   // lo pide thermal_control
unsigned long lastCicloInv = 0;

// Valor físico -> valor crudo del registro (ej: 95.5 % con escala 0.1 -> 955)
int16_t aRegistro(float valor, float escala) {
    return (int16_t)lroundf(valor / escala);
}

// 1) Lee el reg 32 y publica grid_ok si cambió. Devuelve false si el inversor no respondió.
bool leerEstadoInversor() {
    static uint8_t fallas = 0;
    static int8_t  grid_publicado = -1;          // -1: todavía no se publicó
    int16_t reg_32;

    if (inverterRead(REG_STATUS, REG_STATUS_COUNT, &reg_32)) {
        fallas  = 0;
        grid_ok = (reg_32 & 0x0008) && !(reg_32 & 0x0010);
        inv_ok  = (reg_32 & 0x0004) && !(reg_32 & 0x0001);
    } else {
        if (fallas < 5) fallas++;
        if (fallas < 5) return false;            // fallas sueltas: no se cambia nada
        grid_ok = false;                         // 5 seguidas: se avisa que no hay red
        inv_ok  = false;
    }

    if (grid_ok != grid_publicado && publishGridOk(grid_ok)) grid_publicado = grid_ok;
    return fallas == 0;
}

// 2) Máquina de estados
void actualizarEstado() {
    switch (STATE) {
        case IDLE:
            if      (vehiculo_conectado)                        STATE = CHARGING_VEHICLE;
            else if (grid_ok && bms.soc_pct < SOC_INICIO_CARGA) STATE = CHARGING_BATTERY;
            break;

        case CHARGING_VEHICLE:
            if (!vehiculo_conectado)                            STATE = IDLE;
            break;

        case CHARGING_BATTERY:
            if      (vehiculo_conectado)                        STATE = CHARGING_VEHICLE;
            else if (!grid_ok || bms.soc_pct >= SOC_FIN_CARGA)  STATE = IDLE;
            break;
    }
}

// 3) Escribe lo mismo que el EMS original: todo cada 1 s y 329/334/351 cada 10 s.
void escribirInversor() {
    static uint8_t ciclo   = 0;
    static bool    reserva = false;

    // Cada 10 s (y en el primer ciclo). Sin 329/334 el inversor no acepta los 6000.
    if (ciclo == 0) {
        inverterWrite(REG_BMS_MASTER_SLAVE, 1);
        inverterWrite(REG_BMS_POWER_SCHED,  1);
        inverterWrite(REG_OFFGRID_PWR_REG,  2);
    }
    if (++ciclo >= 10) ciclo = 0;

    // Reserva con histéresis: mientras dura, no se descarga la batería
    if      (bms.soc_pct <= SOC_RESERVA)     reserva = true;
    else if (bms.soc_pct >= SOC_FIN_RESERVA) reserva = false;

    bool llena  = bms.soc_pct >= 100.0f;
    bool apagar = apagado_termico || (reserva && !grid_ok);   // en isla con reserva: se apaga

    // 353: cargando (vehículo o batería) se toman limit_pot kW de la red (negativo = importar).
    // En IDLE, apagado o en isla: 0.
    float setpoint_kw = (STATE != IDLE && inv_ok && grid_ok) ? -limit_pot : 0.0f;

    // Cada 1 s, en el mismo orden que el EMS
    inverterWrite(apagar ? REG_SHUTDOWN : REG_POWER_ON, 1);
    inverterWrite(BMS_BATTERY_SOC,           aRegistro(bms.soc_pct, SCALE_SOC_SOH));
    inverterWrite(BMS_MAX_CHG_CURRENT,       llena   ? 0 : aRegistro(bms.max_charge_a,    SCALE_CURRENT_A));
    inverterWrite(BMS_MAX_DISCHG_CURRENT,    reserva ? 0 : aRegistro(bms.max_discharge_a, SCALE_CURRENT_A));
    inverterWrite(REG_DC_MAX_DISCHG_CURRENT, reserva ? 0 : aRegistro(inverter_values.dc_max_dischg_current, SCALE_CURRENT_A));
    inverterWrite(REG_DC_MAX_CHG_CURRENT,    llena   ? 0 : aRegistro(inverter_values.dc_max_chg_current,    SCALE_CURRENT_A));
    inverterWrite(REG_3PHASE_CTRL_MODE,      inverter_values.three_phase_ctrl_mode_value);
    inverterWrite(REG_PV_SWITCH,             inverter_values.pv_switch_value);
    inverterWrite(REG_ANTI_BACKFLOW,         inverter_values.anti_backflow_value);
    inverterWrite(REG_LEAKAGE_DETECT,        inverter_values.leakage_detect_value);
    inverterWrite(REG_DCDC_SWITCH,           inverter_values.dcdc_switch_value);
    inverterWrite(REG_SET_POWER,             aRegistro(setpoint_kw, SCALE_SET_POWER_KW));
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

// thermal_control llama a esto para reportar reducción de potencia / shutdown.
// Publica a thingsboard dos booleanos para poder hacer la logica
// de si hay que avisarle a EveMove que hay que bajar la potencia o directamente cortar
void publicarEstadoCooling(bool bajar_pot, bool shut_down) {
    publishCoolingAttributes(bajar_pot, shut_down);
};

void parser(can_frame* ptr_msg){
    uint16_t id = (uint16_t)((*ptr_msg).can_id);
    bms_parse_can(id, (*ptr_msg).data, bms);
}

// ---------------------------------------------------------------------------
// Aqui terminan los callbacks
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Funciones de Inicializacion
// ---------------------------------------------------------------------------

void init_intervals() {
    intervals.listen_bms_ms  = DEFAULT_LISTEN_BMS_MS;
    intervals.poll_modbus_ms = DEFAULT_POLL_MODBUS_MS;
    intervals.verify_init_ms = DEFAULT_VERIFY_INIT_MS;
    intervals.poll_gas_alarm_ms = DEFAULT_GAS_ALARM_MS;
}

void init(){
    SPI.begin();
    // Se inicializan las uart independientes para el inversor y la alarma.
    INVERTER_SERIAL.begin(INVERTER_BAUD, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN);
    GAS_SERIAL.begin(GAS_BAUD, SERIAL_8N1, GAS_RX_PIN, GAS_TX_PIN);

    inverterInit(INVERTER_SERIAL, INVERTER_DE_RE_PIN);
    bmsCanInit(modoCan);
    dhtInit(DHT_PIN);
    gasAlarmInit(GAS_SERIAL, GAS_DE_RE_PIN);
    connectWiFi();
    connectMQTT(Callback_setup);
}

void init_all_defaults(){
    inverter_init_defaults();
    thermal_thresholds_init_defaults();
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
    publishTelemetryInv(datosInv, "BMSData");
}


void intervals_update(const String& key, float value) {
    if      (key == "listen_bms_ms")  intervals.listen_bms_ms  = (uint32_t)value;
    else if (key == "poll_modbus_ms") intervals.poll_modbus_ms = (uint32_t)value;
    else if (key == "verify_init_ms") intervals.verify_init_ms = (uint32_t)value;
    else if (key == "poll_gas_alarm_ms") intervals.poll_gas_alarm_ms = (uint32_t)value;
    else return;
}

// Las funciones a continuacion se definen aca pero son las que se pasan en los callbacks definidos en el modulo de telemetria.

void telemetria_set_attribute_handler1(const String& key, float value) {
    thermal_update_threshold(key, value);
    inverter_update_reg_values(key, value);
    intervals_update(key, value);
}

void telemetria_set_attribute_handler2(const String& key, float value) {
    thermal_update_threshold(key, value);
    inverter_update_limit_pot(key,value);
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

    // Se envia el frame que hace que el bms comience a mandar periodicamente sus paquetes
    bmsSend(id, datos, dlc, &tx);
}

int8_t num_bms_frames = 9;
int16_t batch_timeout = 1500;
int16_t reg_32 = 0;

// Se definen flags a usar en el bucle principal
bool vehiculo_conectado = false;
bool grid_ok = true;
bool pot_menor_20k_send = false;

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
        // Se llama a la funcion de pasamanos para mandarle al inversor los datos del bms (registros 6000s)
    }


    // Se hace un poll al inversor para obtener datos
    if (millis() - lastPollInv > intervals.poll_modbus_ms){
        lastPollInv = millis();
        pollModbus(datosInv);
        publicarTelemetriaInv();
    }

    // Se lee la alarma de gas
    if (millis() - lastPollGasAlarm > intervals.poll_gas_alarm_ms){
        lastPollGasAlarm  = millis();
        gasAlarmReadAll(G_alarm);

        // Se imprimen los valores del sensor de alarma, si todo marcha, se pueden mandar por telemetria
        Serial.println("----------- Valores gas alarm --------------");
        Serial.println(G_alarm.alarm1_point);
        Serial.println(G_alarm.alarm2_point);
        Serial.println(G_alarm.range);
        Serial.println(G_alarm.resolution);
        Serial.println(G_alarm.unit);
        Serial.println(G_alarm.gas_type);
        Serial.println(G_alarm.concentration);
        Serial.println(G_alarm.alarm_status);
    }

    //-------------------------------------------------
    // Se leen los datos del sensor + Control Termico
    //-------------------------------------------------
    
    T_sensor = dhtRead();
    publishTelemetrySensor(T_sensor.temperature_c, T_sensor.humidity_pct);
    if (thermalControlUpdate(millis(), bms.temp_cell_min_c, bms.temp_cell_max_c, T_sensor.temperature_c, publicarEstadoCooling)){
        inverterWrite(REG_SHUTDOWN, 1);
    }


    //--------------------------
    // Se lee el registro 32
    //--------------------------

    // Independientemente del estado en el que se encuentra el sistema, si la red se desconecta pasamos a modo isla.
    // bit3 = reg_32 & 0x0008 y bit4 = reg_32 & 0x0010
    // bit3 = 'Grid connection Status' / bit4 = 'Off-grid status'
    if (inverterRead(REG_STATUS, 1, &reg_32)){
        grid_ok = ((reg_32 & 0x0008) && !(reg_32 & 0x0010));
    }

    //-------------------------------------------------------------
    // Se hace la pregunta a Evemove de si hay vehiculo conectado
    //-------------------------------------------------------------
    
    // ... completar cuando se tenga hecho
    //vehiculo_conectado = hay_vehiculo();


    switch (STATE) {
        case IDLE:
            // Tambien se pueden cargar las baterias debido al consumo en reposo

            // Se actualizan los estados
            if (vehiculo_conectado)
                STATE = CHARGING_VEHICLE;
            else
                STATE = IDLE;
            break;

        case CHARGING_VEHICLE:
            if (!grid_ok){
                if (!pot_menor_20k_send){
                    pot_menor_20k_send = true;
                    // Se envia a thingsboard el comando para que el cargador pida 20kW o menos 
                }
                if (bms.soc_pct > 10){
                    // Se carga con normalidad usando las baterias
                }else{
                    // Se tiene que suspender la carga de alguna manera, tal vez avisandole al cargador que pare.
                }
            }else{ //grid_ok = true
                pot_menor_20k_send = false;
                // Se puede pedir la potencia demendada por el auto instante a instante y ver como fijar el setpoint (reg.353)
            }

            // Se actualizan los estados
            if (vehiculo_conectado)
                STATE = CHARGING_VEHICLE;
            else if (bms.soc_pct < 95 && grid_ok) // No hay vehiculo y la carga usó las baterias
                    STATE = CHARGING_BATTERY;
                 else
                    STATE = IDLE;
            break;

        case CHARGING_BATTERY:
            // Aca se cargan las baterias siempre que se tenga red, se puede hacer un CCCV o el perfil que se quiera.

            // Se actualizan los estados.
            if (vehiculo_conectado)
                STATE = CHARGING_VEHICLE;
            else if (bms.soc_pct < 95 && grid_ok)
                    STATE = CHARGING_BATTERY;
                 else
                    STATE = IDLE;
            break;
    }
}