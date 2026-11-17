#pragma once
#include <Arduino.h>

typedef struct {
    float cool_on_c;        // T_amb por encima de esto: prenden los ventiladores (MONITOR -> COOLING)
    float cool_off_c;       // T_amb por debajo de esto: se apagan los ventiladores (COOLING -> MONITOR)
    float cool_pot_c;       // T_amb desde acá: se pide reducir la potencia
    float cool_critical_c;  // T_amb desde acá: se apaga el inversor
} CoolingThresholds;

enum CoolingState {
    COOLING_STATE_MONITOR,
    COOLING_STATE_COOLING
};

extern CoolingThresholds cooling_thresholds; // Extern para que se pueda acceder desde cooling_control.cpp y desde el main.

// Configura los pines de los relés y deja las heating plates apagadas. Llamar una vez en setup().
void coolingControlInit();

// Inicializa los umbrales con los valores de default
void cooling_thresholds_init_defaults(void);

// Los modifica de acuerdo a lo que se envia desde thingsboard
void cooling_update_threshold(const String& key, float value);

// Llamar periódicamente con la temperatura ambiente.
// Devuelve true si hay que apagar el inversor (temperatura crítica).
bool coolingControlUpdate(float temp_amb, void (*mandarT)(const char*, bool));

// Para debug / display / telemetría.
CoolingState coolingControlGetState();