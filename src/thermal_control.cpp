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

bool thermalControlUpdate(unsigned long now_ms, float temp_min, float temp_max, float temp_amb, void (*mandarT)(float, bool, bool)) {
    //if (!bms.valid) return;  // sin dato fresco del BMS, no se toca nada

    bool shut_down = false;

    switch (state) {
        case THERMAL_STATE_MONITOR:
            if (temp_min < TEMP_HEAT_ENTER_C) {
                enterHeating(now_ms);
            } else if (temp_amb > TEMP_COOL_ON_C) {
                enterCooling();
            }
            break;

        case THERMAL_STATE_HEATING: {
            // Salida principal: T_min ya se recuperó -> vuelve a MONITOR,
            // sin importar en qué sub-fase (ON u OFF) esté el ciclo.
            if (temp_min > TEMP_HEAT_EXIT_C) {
                enterMonitor();
                break;
            }

            // Timeout de seguridad: si por alguna falla T_min nunca se
            // recupera, no se queda calentando para siempre.
            unsigned long elapsedTotal = now_ms - heatingStartMs;
            if (elapsedTotal >= HEATING_TIMEOUT_MS) {
                enterMonitor();
                break;
            }


            if (heatingPlateOn) {
                // Fase ON: se apaga T_max ya llegó al límite local de la placa.
                if (temp_max >= TEMP_PLATE_MAX_C) {
                    setHeatingPlates(false);
                    heatingCycleMarkMs = now_ms;
                }
            } else {
                // Fase OFF: recién vuelve a prender cuando la placa se enfrió lo
                // suficiente (histéresis contra T_UMBRAL_3).
                if (temp_max <= TEMP_PLATE_RESUME_C) {
                    setHeatingPlates(true);
                    heatingCycleMarkMs = now_ms;
                }
            }
            break;
        }

        case THERMAL_STATE_COOLING:
            if (temp_amb < TEMP_COOL_OFF_C){
                enterMonitor();
                mandarT(temp_amb, false, false);
            } else if (temp_amb >= TEMP_COOL_OFF_C && temp_amb < TEMP_COOL_POT){
                mandarT(temp_amb, false, false);    
            }else if (temp_amb >= TEMP_COOL_POT && temp_amb < TEMP_CRITICAL_C){
                mandarT(temp_amb, true, false);
            }else if (temp_amb >= TEMP_CRITICAL_C){
                mandarT(temp_amb, false, true);
                shut_down = true;
            }
            break;
    }
    return shut_down;
}
