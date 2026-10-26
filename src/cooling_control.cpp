#include "cooling_control.h"
#include "config.h"
#include <Arduino.h>

CoolingThresholds cooling_thresholds;

namespace {
    CoolingState state = COOLING_STATE_MONITOR;

    void setFans(bool on) {
        digitalWrite(FAN_RELAY_PIN, on ? HIGH : LOW);
    }

    void enterMonitor() {
        setFans(false);
        state = COOLING_STATE_MONITOR;
    }

    void enterCooling() {
        setFans(true);
        state = COOLING_STATE_COOLING;
    }
}


void coolingControlInit() {
    pinMode(FAN_RELAY_PIN, OUTPUT);
    pinMode(HEATING_RELAY_PIN, OUTPUT);
    digitalWrite(HEATING_RELAY_PIN, LOW);   // las heating plates quedan siempre apagadas
    enterMonitor();
}

CoolingState coolingControlGetState() {
    return state;
}

void cooling_thresholds_init_defaults(){
    cooling_thresholds.cool_on_c = DEFAULT_TEMP_COOL_ON_C;
    cooling_thresholds.cool_off_c = DEFAULT_TEMP_COOL_OFF_C;
    cooling_thresholds.cool_pot_c = DEFAULT_TEMP_COOL_POT;
    cooling_thresholds.cool_critical_c = DEFAULT_TEMP_COOL_CRITICAL_C;
}

void cooling_update_threshold(const String& key, float value) {
    if      (key == "cool_on_c")       cooling_thresholds.cool_on_c = value;
    else if (key == "cool_off_c")      cooling_thresholds.cool_off_c = value;
    else if (key == "cool_pot_c")      cooling_thresholds.cool_pot_c = value;
    else if (key == "cool_critical_c") cooling_thresholds.cool_critical_c = value;
    else return; // key que no le corresponde a este módulo, se ignora

    //Serial.print("[COOLING] ");
    //Serial.print(key);
    //Serial.print(" = ");
    //Serial.println(value);
}


bool coolingControlUpdate(float temp_amb, void (*mandarT)(const char*, bool)) {
    bool shut_down = false;

    switch (state) {
        case COOLING_STATE_MONITOR:
            if (temp_amb > cooling_thresholds.cool_on_c) {
                enterCooling();
            }
            break;

        case COOLING_STATE_COOLING:
            if (temp_amb < cooling_thresholds.cool_off_c){
                enterMonitor();
                mandarT("reducir_pot_temp", false);
            } else if (temp_amb >= cooling_thresholds.cool_off_c && temp_amb < cooling_thresholds.cool_pot_c){
                mandarT("reducir_pot_temp", false);
            }else if (temp_amb >= cooling_thresholds.cool_pot_c && temp_amb < cooling_thresholds.cool_critical_c){
                mandarT("reducir_pot_temp", true);
            }else if (temp_amb >= cooling_thresholds.cool_critical_c){
                mandarT("reducir_pot_temp", false);
                shut_down = true;
            }
            break;
    }
    return shut_down;
}