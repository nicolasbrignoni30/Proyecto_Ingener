#include "gas_parser.h"
#include <string.h>

float gas_reg_to_float(uint16_t hi, uint16_t lo) {
    uint32_t raw = ((uint32_t)hi << 16) | lo;
    float value;
    memcpy(&value, &raw, sizeof(value)); // evita undefined behavior de castear el puntero directo
    return value;
}

void gas_parse_all(const uint16_t* r, GasData& o) {
    // offsets relativos a GAS_REG_ALARM1 (7000); cada campo ocupa 2 registros
    o.alarm1_point  = gas_reg_to_float(r[0],  r[1]);
    o.alarm2_point  = gas_reg_to_float(r[2],  r[3]);
    o.range         = gas_reg_to_float(r[4],  r[5]);
    o.resolution    = gas_reg_to_float(r[6],  r[7]);
    o.unit          = (uint8_t)gas_reg_to_float(r[8],  r[9]);
    o.gas_type      = (uint8_t)gas_reg_to_float(r[10], r[11]);
    o.concentration = gas_reg_to_float(r[12], r[13]);
    o.alarm_status  = (uint8_t)gas_reg_to_float(r[14], r[15]);
}