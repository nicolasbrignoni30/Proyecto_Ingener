#pragma once
// =============================================================================
// config.h — Parámetros configurables del sistema
// =============================================================================

// WiFi — credenciales en credentials.h
// Comente esto del credentials porque me estaba dando error, despues se lo pregunto bien a Andres.
//#include "credentials.h"  // nunca subir al repo — ver credentials.h.example

// ThingsBoard
#define TB_HOST          "thingsboard.cloud"
#define TB_PORT          1883


// ---------------------------------------------------------------------------
// CONFIGURACIÓN INTERNA DE HARDWARE Y REGISTROS
// ---------------------------------------------------------------------------

// Modubus RTU del inversor
#define INVERTER_BAUD       115200
#define INVERTER_SERIAL     Serial2
#define INVERTER_TX_PIN     17
#define INVERTER_RX_PIN     16
#define INVERTER_DE_RE_PIN  4
#define MODBUS_DEVICE_ID 1

// Intervalos de polling (ms)
#define DEFAULT_POLL_MODBUS_MS   5000
#define DEFAULT_VERIFY_INIT_MS   60000

#define DEFAULT_DC_MAX_DISCHG_CURRENT 90.0f
#define DEFAULT_DC_MAX_CHG_CURRENT 40.0f
#define DEFAULT_ANTI_BACKFLOW_VALUE 1
#define DEFAULT_SCHED_MODE_VALUE 0
#define DEFAULT_THREE_PHASE_CTRL_MODE_VALUE 1
#define DEFAULT_PV_SWITCH_VALUE 0
#define DEFAULT_LEAKAGE_DETECT_VALUE 0
#define DEFAULT_DCDC_SWITCH_VALUE 0
#define DEFAULT_SET_POWER -20.0f //Ver si es positivo o negativo
#define DEFAULT_POWER_ON_VALUE 1

#define CANT_REG_INIT 10
    

// Protocolo Modbus del inversor
// Descomentar si el firmware es >= V3.0 — habilita lectura de regs 200-213 (load)
// El firmware actual es V2.88 — dejar comentado
// #define INVERTER_PROTOCOL_V3

// Modbus RTU de la alarma de gas
#define GAS_BAUD         9600
#define GAS_SERIAL       Serial1
#define GAS_TX_PIN       27
#define GAS_RX_PIN       26
#define GAS_DE_RE_PIN    25
#define GAS_DEVICE_ID    1

#define DEFAULT_GAS_ALARM_MS 5000

// Pines del Módulo CAN MCP2515
#define DEFAULT_LISTEN_BMS_MS  5000
#define CAN_CS    15   // Pin Chip Select exclusivo para el MCP2515
#define CAN_INT   22   // Pin de Interrupción para que el MCP2515 te avise de datos nuevos
// Los demas pines como MOSI (23), MISO (19), y SCK (18) estan definidos por defecto.

// pin para el sensor de temperatura y humedad
#define DHT_PIN 33 

// =============================================================================
// Pines para la pantalla TFT 1.8" SPI ST7735 
// =============================================================================
// Esto cambia con respecto a la de Ideaspark pues ahora la pantalla no está integrada a la placa y la conexion es externa.
#define TFT_MOSI     23  // Pin SDA / MOSI de la pantalla -> Conectar a GPIO 23 (IOMUX VSPI Nativo)
#define TFT_SCLK     18  // Pin SCL / CLK de la pantalla  -> Conectar a GPIO 18 (IOMUX VSPI Nativo)
#define TFT_CS        5  // Pin CS de la pantalla         -> Conectar a GPIO 5
#define TFT_DC        2  // Pin A0 / DC de la pantalla    -> Conectar a GPIO 2
#define TFT_RST       4  // Pin RESET de la pantalla     -> Conectar a GPIO 4

// LED de status — GPIO2 (LED integrado NodeMCU, activo en LOW)
#define LED_PIN          2
#define LED_ON()         digitalWrite(LED_PIN, LOW)
#define LED_OFF()        digitalWrite(LED_PIN, HIGH)

// ---------------------------------------------------------------------------
// Control térmico de baterías — ventiladores y heating plates
// ---------------------------------------------------------------------------
// Relé de ventiladores — un GPIO controla los 4 ventiladores juntos (circuito ya validado)
#define FAN_RELAY_PIN         14
 
// Relé(s) de heating plates — un GPIO controla ambas plaquetas juntas
// TODO: confirmar si es realmente un solo pin para las dos, o hace falta un
// segundo GPIO porque a veces se controlan por separado.
#define HEATING_RELAY_PIN     12
 

// Aca van los valores default para los umbrales de temperatura
#define DEFAULT_TEMP_HEAT_ENTER_C     10.0f
#define DEFAULT_TEMP_HEAT_EXIT_C      20.0f
#define DEFAULT_TEMP_PLATE_MAX_C      50.0f
#define DEFAULT_TEMP_PLATE_MIN_C      35.0f  // el delta es de 15 grados como dijo el Seba

#define DEFAULT_TEMP_COOL_ON_C        40.0f
#define DEFAULT_TEMP_COOL_OFF_C       30.0f
#define DEFAULT_TEMP_COOL_POT         55.0f
#define DEFAULT_TEMP_COOL_CRITICAL_C       60.0f


// Si hace falta se pueden considerar timeouts pero hay que estudiarlos con cuidado
 
// Ciclo de calentamiento (estado HEATING)
// #define HEATING_ON_MIN          1     // minutos máx. de la fase ON por ciclo
// #define HEATING_OFF_MIN         2     // minutos mín. de la fase OFF por ciclo
// #define HEATING_TIMEOUT_MIN    10     // timeout de seguridad: tiempo máx. total en HEATING
//                                        // (por si T_min nunca sube por falla de sensor u otra causa)
 
// #define HEATING_ON_MS        (HEATING_ON_MIN     * 60UL * 1000UL)
// #define HEATING_OFF_MS       (HEATING_OFF_MIN    * 60UL * 1000UL)
// #define HEATING_TIMEOUT_MS   (HEATING_TIMEOUT_MIN * 60UL * 1000UL)
 
 