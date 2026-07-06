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
#define RS485_BAUD       115200
#define RS485_SERIAL     Serial2
#define RS485_TX_PIN     17
#define RS485_RX_PIN     16
#define RS485_DE_RE_PIN  4
#define MODBUS_DEVICE_ID 1

// Protocolo Modbus del inversor
// Descomentar si el firmware es >= V3.0 — habilita lectura de regs 200-213 (load)
// El firmware actual es V2.88 — dejar comentado
// #define INVERTER_PROTOCOL_V3

// Pines del Módulo CAN MCP2515
#define CAN_CS    15   // Pin Chip Select exclusivo para el MCP2515
#define CAN_INT   22   // Pin de Interrupción para que el MCP2515 te avise de datos nuevos

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

// Intervalos de polling (ms)
#define POLL_MODBUS_MS   5000
#define POLL_BMS_MS      2000
#define PUBLISH_MS       10000
#define VERIFY_INIT_MS   60000

// Registros Modbus — SP6030 protocolo V3.0
// Las definiciones de registros de control están en lib/inverter_parser/src/inverter_parser.h
// Lectura
#define REG_STATUS                  32
#define REG_STATUS_COUNT             1
#define REG_AC_START               100
#define REG_AC_COUNT                26
#define REG_DC_START               141
#define REG_DC_COUNT                 3
#define REG_GRID_START             170
#define REG_GRID_COUNT              10
#define REG_GRID_POWER             192
#define REG_LOAD_START             200
#define REG_LOAD_COUNT              14
#define REG_VERSION_START            0
#define REG_VERSION_COUNT           22
