#pragma once
#include <stdint.h>

// =============================================================================
// gas_parser — decodificación pura de registros del sensor de gas (Modbus RTU)
// =============================================================================

#define GAS_REG_ALARM1         7000  // Alarma nivel I (baja)      R/W
#define GAS_REG_ALARM2         7002  // Alarma nivel II (alta)     R/W
#define GAS_REG_RANGE          7004  // Rango de medición          R
#define GAS_REG_RESOLUTION     7006  // Resolución                 R
#define GAS_REG_UNIT           7008  // Unidad                     R
#define GAS_REG_GAS_TYPE       7010  // Tipo de gas                R
#define GAS_REG_CONCENTRATION  7012  // Concentración actual       R
#define GAS_REG_STATUS         7014  // Estado de alarma           R
#define GAS_REG_ZERO_AD        7018  // AD de cero (calibración)   R/W
#define GAS_REG_FULL_AD        7020  // AD de fondo de escala      R/W
#define GAS_REG_DEVICE_ADDR    7022  // Dirección del dispositivo  R/W (16-bit, no float)

#define GAS_REG_ALL_COUNT        16  // 7000..7015, lectura del bloque completo (8 floats)

enum GasUnit {
    GAS_UNIT_PCT_VOL = 0,
    GAS_UNIT_PPM     = 1,
    GAS_UNIT_PCT_LEL = 2,
    GAS_UNIT_CELSIUS = 3
};

enum GasType {
    GAS_TYPE_O2 = 0, GAS_TYPE_CO = 1, GAS_TYPE_H2S = 2, GAS_TYPE_NH3 = 3,
    GAS_TYPE_H2 = 4, GAS_TYPE_CL2 = 5, GAS_TYPE_SO2 = 6, GAS_TYPE_NO = 7,
    GAS_TYPE_NO2 = 8, GAS_TYPE_HCHO = 9, GAS_TYPE_O3 = 10, GAS_TYPE_LEL = 11,
    GAS_TYPE_CO2 = 15
};

enum GasAlarmStatus {
    GAS_STATUS_NORMAL      = 0,
    GAS_STATUS_ALARM_LOW   = 1,   // nivel I
    GAS_STATUS_ALARM_HIGH  = 2,   // nivel II
    GAS_STATUS_MALFUNCTION = 3,
    GAS_STATUS_OVER_RANGE  = 4
};

struct GasData {
    float   alarm1_point;
    float   alarm2_point;
    float   range;
    float   resolution;
    uint8_t unit;          // ver GasUnit
    uint8_t gas_type;      // ver GasType
    float   concentration;
    uint8_t alarm_status;  // ver GasAlarmStatus
};

// Combina 2 registros de 16 bits (orden ABCD, sin word-swap, según el
// protocolo) en un float IEEE754 de 32 bits.
float gas_reg_to_float(uint16_t hi, uint16_t lo);

// raw[] = los GAS_REG_ALL_COUNT registros leídos a partir de GAS_REG_ALARM1 (7000).
void gas_parse_all(const uint16_t* raw, GasData& out);
