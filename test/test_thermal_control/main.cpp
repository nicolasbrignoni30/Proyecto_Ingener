// test_thermal_control — Verificación de la máquina de estados térmica
//
// Qué hace:
//   Simula distintos escenarios de temperatura del BMS y hace avanzar el
//   tiempo "a mano" (sin esperar los minutos reales) para verificar las
//   transiciones MONITOR / HEATING / COOLING, incluyendo la lógica de dos
//   niveles dentro de HEATING (corte local por T_max, histéresis, salida
//   por T_min, timeout de seguridad). Mirar el Monitor Serie a 115200.
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
// Escenarios — HEATING
// ---------------------------------------------------------------------------
void escenario_heating_ciclo_normal() {
    Serial.println("\n=== 1: entra a HEATING, ciclo ON/OFF normal (T_max bajo todo el tiempo) ===");
    unsigned long simMs = 0;
    BmsData bms = bmsFake(/*tMin=*/-2.0f, /*tMax=*/5.0f); // T_max lejos de TEMP_PLATE_MAX_C

    thermalControlUpdate(bms, simMs);            // T_min < TEMP_HEAT_ENTER_C -> HEATING, plate ON
    imprimirEstado(simMs, bms);

    avanzar(simMs, HEATING_ON_MS / 2, bms);      // sigue ON (no llegó tiempo ni T_max)
    avanzar(simMs, HEATING_ON_MS / 2 + 1, bms);  // cruza HEATING_ON_MS -> se apaga
    avanzar(simMs, HEATING_OFF_MS / 2, bms);     // sigue OFF (no pasó HEATING_OFF_MS)
    avanzar(simMs, HEATING_OFF_MS / 2 + 1, bms); // ya pasó HEATING_OFF_MS y T_max <= UMBRAL_3 -> prende de nuevo
}

void escenario_heating_corte_temprano_por_plate() {
    Serial.println("\n=== 2: corte ANTES de tiempo — T_max llega a TEMP_PLATE_MAX_C durante la fase ON ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, simMs);        // entra a HEATING, plate ON
    imprimirEstado(simMs, bmsFrio);

    // Avanza poco tiempo (bien antes de HEATING_ON_MS) pero con T_max ya alto
    BmsData bmsPlacaCaliente = bmsFake(-2.0f, TEMP_PLATE_MAX_C + 0.5f);
    avanzar(simMs, HEATING_ON_MS / 4, bmsPlacaCaliente); // debería apagar YA, sin esperar el tiempo
}

void escenario_heating_resume_con_histeresis() {
    Serial.println("\n=== 3: fase OFF no resume hasta que T_max < TEMP_PLATE_RESUME_C (histéresis) ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, simMs);        // entra a HEATING, plate ON

    // Fuerza el apagado temprano por T_max alto
    BmsData bmsPlacaCaliente = bmsFake(-2.0f, TEMP_PLATE_MAX_C + 0.5f);
    avanzar(simMs, 1000, bmsPlacaCaliente);      // se apaga (fase OFF empieza acá)

    // Ya pasó HEATING_OFF_MS, pero T_max sigue arriba de TEMP_PLATE_RESUME_C -> NO debería prender
    avanzar(simMs, HEATING_OFF_MS + 1000, bmsPlacaCaliente);

    // Ahora sí baja T_max por debajo de TEMP_PLATE_RESUME_C -> recién ahí prende
    BmsData bmsPlacaEnfriada = bmsFake(-2.0f, TEMP_PLATE_RESUME_C - 0.5f);
    avanzar(simMs, 1000, bmsPlacaEnfriada);
}

void escenario_heating_sale_por_tmin() {
    Serial.println("\n=== 4: sale de HEATING cuando T_min > TEMP_HEAT_EXIT_C (sin importar la fase ON/OFF) ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, simMs);        // entra a HEATING

    avanzar(simMs, HEATING_ON_MS / 3, bmsFrio);  // sigue en HEATING, en fase ON

    BmsData bmsRecuperado = bmsFake(TEMP_HEAT_EXIT_C + 0.5f, 5.0f);
    avanzar(simMs, 1000, bmsRecuperado);         // debería volver a MONITOR ya
}

void escenario_heating_timeout_seguridad() {
    Serial.println("\n=== 5: timeout de seguridad — T_min nunca se recupera ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, simMs);        // entra a HEATING

    avanzar(simMs, HEATING_TIMEOUT_MS + 1, bmsFrio); // debería forzar MONITOR por timeout, no por T_min
}

// ---------------------------------------------------------------------------
// Escenarios — COOLING y override de seguridad
// ---------------------------------------------------------------------------
void escenario_cooling() {
    Serial.println("\n=== 6: caliente -> COOLING -> se enfría -> MONITOR ===");
    unsigned long simMs = 0;
    BmsData bmsCaliente = bmsFake(/*tMin=*/30.0f, /*tMax=*/50.0f);

    thermalControlUpdate(bmsCaliente, simMs);        // debería pasar a COOLING (fans ON)
    imprimirEstado(simMs, bmsCaliente);

    BmsData bmsEnfriando = bmsFake(30.0f, 38.0f);    // por debajo de TEMP_COOL_OFF_C
    avanzar(simMs, 1000, bmsEnfriando);              // debería volver a MONITOR (fans OFF)
}

void escenario_critico_durante_heating() {
    Serial.println("\n=== 7: falla durante HEATING -> override de seguridad ===");
    unsigned long simMs = 0;
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, simMs);            // entra a HEATING
    imprimirEstado(simMs, bmsFrio);

    BmsData bmsFalla = bmsFake(-2.0f, TEMP_CRITICAL_C + 1.0f); // salto anómalo
    avanzar(simMs, 5000, bmsFalla);                  // debería forzar COOLING pese a estar en HEATING
}

// ---------------------------------------------------------------------------
// Escenario 8: TIEMPO REAL — para sentir/escuchar los relés físicamente.
// A diferencia de los escenarios de arriba (que saltan el tiempo a mano y
// sirven solo para verificar la lógica), este usa millis() real. Con los
// valores actuales de config.h (1 min ON / 2 min OFF) vas a poder escuchar
// el relé de heating prender y apagar en tiempo real.
// Se llama desde loop(), no desde setup().
// ---------------------------------------------------------------------------
bool tiempoRealIniciado = false;
unsigned long tiempoRealUltimoPrint = 0;

void escenario_tiempo_real_step() {
    unsigned long ahora = millis();

    if (!tiempoRealIniciado) {
        Serial.println("\n=== 8: TIEMPO REAL (millis real) — frío, debería entrar a HEATING ===");
        thermalControlInit();
        tiempoRealIniciado = true;
        tiempoRealUltimoPrint = ahora;
    }

    // BMS simulado "frío" fijo, con T_max bajo (no dispara el corte por placa),
    // para sostener el ciclo ON/OFF indefinidamente y poder escucharlo.
    BmsData bmsFrio = bmsFake(-2.0f, 5.0f);
    thermalControlUpdate(bmsFrio, ahora);

    // Imprime una vez por segundo para no inundar el Monitor Serie
    if (ahora - tiempoRealUltimoPrint >= 1000) {
        imprimirEstado(ahora, bmsFrio);
        tiempoRealUltimoPrint = ahora;
    }
}

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[Boot] test_thermal_control starting...");

    thermalControlInit();

    Serial.println("--- Escenarios 1-7: verificación rápida de lógica (tiempo simulado) ---");
    Serial.println("    Los relés van a clickear muy rápido acá, no representa tiempos reales.");

    escenario_heating_ciclo_normal();
    thermalControlInit();
    escenario_heating_corte_temprano_por_plate();
    thermalControlInit();
    escenario_heating_resume_con_histeresis();
    thermalControlInit();
    escenario_heating_sale_por_tmin();
    thermalControlInit();
    escenario_heating_timeout_seguridad();
    thermalControlInit();
    escenario_cooling();
    thermalControlInit();
    escenario_critico_durante_heating();

    Serial.println("\n[FIN escenarios 1-7] Ahora arranca el escenario 8 (tiempo real) en loop()...");
}

void loop() {
    escenario_tiempo_real_step();
}