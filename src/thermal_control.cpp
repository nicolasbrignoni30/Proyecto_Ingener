#include "thermal_control.h"
#include "config.h"
#include <Arduino.h>

namespace {
    ThermalState state = THERMAL_STATE_MONITOR;
    unsigned long heatingStartMs = 0;      // inicio del estado HEATING (para el tiempo total)
    unsigned long heatingCycleMarkMs = 0;  // inicio del sub-ciclo ON/OFF actual
    bool heatingPlateOn = false;

    void setFans(bool on) {
        digitalWrite(FAN_RELAY_PIN, on ? HIGH : LOW);
    }

    void setHeatingPlates(bool on) {
        digitalWrite(HEATING_RELAY_PIN, on ? HIGH : LOW);
        heatingPlateOn = on;
    }

    void enterMonitor() {
        setHeatingPlates(false);
        setFans(false);
        state = THERMAL_STATE_MONITOR;
    }

    void enterHeating(unsigned long now_ms) {
        heatingStartMs = now_ms;
        heatingCycleMarkMs = now_ms;
        setFans(false);
        setHeatingPlates(true);  // arranca el ciclo en ON
        state = THERMAL_STATE_HEATING;
    }

    void enterCooling() {
        setHeatingPlates(false);
        setFans(true);
        state = THERMAL_STATE_COOLING;
    }
}

void thermalControlInit() {
    pinMode(FAN_RELAY_PIN, OUTPUT);
    pinMode(HEATING_RELAY_PIN, OUTPUT);
    enterMonitor();
}

ThermalState thermalControlGetState() {
    return state;
}

void thermalControlUpdate(const BmsData& bms, unsigned long now_ms) {
    if (!bms.valid) return;  // sin dato fresco del BMS, no se toca nada

    // --- Seguridad absoluta: tiene prioridad sobre cualquier estado,
    //     incluso HEATING. Un valor tan alto ya no es "ruido de la placa". ---
    if (bms.temp_cell_max_c >= TEMP_CRITICAL_C) {
        if (state != THERMAL_STATE_COOLING) enterCooling();
        return;
    }

    switch (state) {
        case THERMAL_STATE_MONITOR:
            if (bms.temp_cell_min_c < TEMP_HEAT_ON_C) {
                enterHeating(now_ms);
            } else if (bms.temp_cell_max_c > TEMP_COOL_ON_C) {
                enterCooling();
            }
            break;

        case THERMAL_STATE_HEATING: {
            // OJO: mientras se calienta, NO se usa la lectura de temperatura
            // para decidir salir de este estado — la termocupla está cerca
            // de la placa y no refleja la temperatura real de la batería.
            unsigned long elapsedTotal = now_ms - heatingStartMs;
            if (elapsedTotal >= HEATING_TOTAL_MS) {
                enterMonitor();
                break;
            }

            unsigned long elapsedCycle = now_ms - heatingCycleMarkMs;
            if (heatingPlateOn && elapsedCycle >= HEATING_ON_MS) {
                setHeatingPlates(false);
                heatingCycleMarkMs = now_ms;
            } else if (!heatingPlateOn && elapsedCycle >= HEATING_OFF_MS) {
                setHeatingPlates(true);
                heatingCycleMarkMs = now_ms;
            }
            break;
        }

        case THERMAL_STATE_COOLING:
            if (bms.temp_cell_max_c < TEMP_COOL_OFF_C) {
                enterMonitor();
            }
            break;
    }
}