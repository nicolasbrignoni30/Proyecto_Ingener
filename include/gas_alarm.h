#pragma once
#include <Arduino.h>
#include "gas_parser.h"


void gasAlarmInit(HardwareSerial& serial, uint8_t deRePin);

// Lectura cruda de registros (words de 16 bits, sin interpretar).
bool gasAlarmRead(uint16_t reg, uint16_t count, uint16_t* out);

// Lee el bloque completo (7000-7015) y lo deja ya parseado en `out`.
bool gasAlarmReadAll(GasData& out);
