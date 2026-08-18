#pragma once

#include "bms_parser.h"

enum ThermalState {
    THERMAL_STATE_MONITOR,
    THERMAL_STATE_HEATING,
    THERMAL_STATE_COOLING
};

// Configura los pines de los relés. Llamar una vez en setup().
void thermalControlInit();

// Llamar periódicamente (ej. cada vez que llega un frame nuevo del BMS,
// o cada POLL_BMS_MS) pasando la última lectura válida y millis() actual.
void thermalControlUpdate(const BmsData& bms, unsigned long now_ms);

// Para debug / display / telemetría.
ThermalState thermalControlGetState();