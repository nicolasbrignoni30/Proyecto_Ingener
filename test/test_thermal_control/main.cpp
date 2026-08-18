// test_thermal_control — Verificación de la máquina de estados térmica
//
// Qué hace:
//   Simula distintos escenarios de temperatura del BMS y hace avanzar el
//   tiempo "a mano" (sin esperar los minutos reales) para verificar las
//   transiciones MONITOR / HEATING / COOLING y el ciclo ON/OFF de las
//   heating plates. Mirar el Monitor Serie a 115200.
//
// No requiere el módulo CAN ni un BMS real conectado — se arma un BmsData
// simulado a mano en cada escenario.

#include <Arduino.h>
#include "thermal_control.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
const char* nombreEstado(ThermalState s) {
    switch (s) {
        case THERMAL_STATE_MONITOR: return "MONITOR";
        case THERMAL_STATE_HEATING: return "HEATING";
        case THERMAL_STATE_COOLING: return "COOLING";
    }
    return "?";
}

void imprimirEstado(unsigned long simMs, const BmsData& bms) {
    Serial.printf(
        "[t=%6lu ms] temp_min=%.1f temp_max=%.1f -> estado=%s | FAN=%d HEAT=%d\n",
        simMs, bms.temp_cell_min_c, bms.temp_cell_max_c, nombreEstado(thermalControlGetState()),
        digitalRead(FAN_RELAY_PIN), digitalRead(HEATING_RELAY_PIN)
    );
}

BmsData bmsFake(float tMin, float tMax) {
    BmsData bms{};
    bms.temp_cell_min_c = tMin;
    bms.temp_cell_max_c = tMax;
    bms.valid = true;
    return bms;
}

void avanzar(unsigned long& simMs, unsigned long deltaMs, const BmsData& bms) {
    simMs += deltaMs;
    thermalControlUpdate(bms, simMs);
    imprimirEstado(simMs, bms);
    delay(300); // solo para poder leer el Monitor Serie con comodidad
}

// ---------------------------------------------------------------------------
// Escenarios
// ---------------------------------------------------------------------------
void escenario_ciclo_heating() {
    Serial.println("\n=== Escenario 1: frío -> HEATING -> ciclos ON/OFF -> timeout -> MONITOR ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(/*tMin=*/-2.0f, /*tMax=*/5.0f);

    thermalControlUpdate(bmsFrio, simMs);           // debería pasar a HEATING (plate ON)
    imprimirEstado(simMs, bmsFrio);

    // Avanza a la mitad del ON, sigue ON
    avanzar(simMs, HEATING_ON_MS / 2, bmsFrio);
    // Cruza el borde de ON -> OFF
    avanzar(simMs, HEATING_ON_MS / 2 + 1, bmsFrio);
    // Cruza el borde de OFF -> ON otra vez
    avanzar(simMs, HEATING_OFF_MS + 1, bmsFrio);
    // Salta cerca del final del tiempo total de HEATING
    avanzar(simMs, HEATING_TOTAL_MS, bmsFrio);       // debería volver a MONITOR
}

void escenario_cooling() {
    Serial.println("\n=== Escenario 2: caliente -> COOLING -> se enfría -> MONITOR ===");
    unsigned long simMs = 0;
    BmsData bmsCaliente = bmsFake(/*tMin=*/30.0f, /*tMax=*/50.0f);

    thermalControlUpdate(bmsCaliente, simMs);        // debería pasar a COOLING (fans ON)
    imprimirEstado(simMs, bmsCaliente);

    BmsData bmsEnfriando = bmsFake(30.0f, 38.0f);    // por debajo de TEMP_COOL_OFF_C
    avanzar(simMs, 1000, bmsEnfriando);              // debería volver a MONITOR (fans OFF)
}

void escenario_critico_durante_heating() {
    Serial.println("\n=== Escenario 3: falla durante HEATING -> override de seguridad ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, simMs);            // entra a HEATING
    imprimirEstado(simMs, bmsFrio);

    BmsData bmsFalla = bmsFake(-2.0f, TEMP_CRITICAL_C + 1.0f); // salto anómalo
    avanzar(simMs, 5000, bmsFalla);                  // debería forzar COOLING pese a estar en HEATING
}

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[Boot] test_thermal_control starting...");

    thermalControlInit();

    escenario_ciclo_heating();
    thermalControlInit(); // reset entre escenarios
    escenario_cooling();
    thermalControlInit();
    escenario_critico_durante_heating();

    Serial.println("\n[FIN] Escenarios completados.");
}

void loop() {
    // No hace falta nada acá — todo corre una vez en setup()
}