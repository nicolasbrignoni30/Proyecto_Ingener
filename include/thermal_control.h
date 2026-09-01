#pragma once
#include <Arduino.h> 

typedef struct {
    float heat_enter_c;
    float heat_exit_c;
    float plate_max_c;
    float plate_min_c; 

    float cool_on_c;
    float cool_off_c;
    float cool_pot_c;
    float cool_critical_c;
} ThermalThresholds;

enum ThermalState {
    THERMAL_STATE_MONITOR,
    THERMAL_STATE_HEATING,
    THERMAL_STATE_COOLING
};

extern ThermalThresholds thermal_thresholds; // Extern para que se pueda acceder desde thermal_control.cpp y desde el main.

// Configura los pines de los relés. Llamar una vez en setup().
void thermalControlInit();

// Inicializa los umbrales con los valores de default
void thermal_thresholds_init_defaults(void);

// Los modifica de acuerdo a lo que se envia desde thingsboard en el setup
void thermal_update_threshold(const String& key, float value);

// Llamar periódicamente (ej. cada vez que llega un frame nuevo del BMS,
// o cada POLL_BMS_MS) pasando la última lectura válida y millis() actual.
bool thermalControlUpdate(unsigned long now_ms, float temp_min, float temp_max, float temp_amb, void (*mandarT)(bool, bool));

// Para debug / display / telemetría.
ThermalState thermalControlGetState();


