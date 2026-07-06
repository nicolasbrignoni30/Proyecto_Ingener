// =============================================================================
// test_tb — Telemetría simulada → ThingsBoard
//
// Qué hace:
//   1. Conecta WiFi y ThingsBoard
//   2. Envía todos los keys de telemetría que manda el firmware real, con valores
//      que derivan lentamente de forma realista — usalo para armar el dashboard TB
//
// Simula: inversor SP6030 (AC, DC, grid, load) + BMS LWS (todos los campos)
// No requiere hardware más allá de WiFi.

#include "telemetria.h"

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[Boot] test_tb starting...");
    randomSeed(esp_random());

    connectWiFi();
    connectMQTT();

    Serial.println("[Boot] Ready");
}

unsigned long lastPublish = 0;
Sim datos_simulados {
    // Inverter
    75.0f,       // soc
    12.0f,       // p_inv
    3.0f,        // grid_p
    9.0f,        // load_p
    50.0f,       // freq
    231.0f,      // v_phase
    0.98f,       // pf

    // BMS — LWS Modbus fields
    496.0f,      // bms_v
    40.0f,       // bms_i
    24.0f,       // bms_temp_avg
    26.0f,       // bms_temp_max
    22.0f,       // bms_temp_min
    28.0f,       // bms_temp_fet
    3.31f,       // bms_cell_v_max
    3.28f,       // bms_cell_v_min
    200.0f,      // bms_max_chg_a
    200.0f,      // bms_max_dischg_a
    537.6f,      // bms_chg_cutoff
    432.0f       // bms_dischg_cutoff
};

void loop() {
    // Reconnect if needed
    if (!checkWiFiConnection()){ 
        connectWiFi(); 
    }
    if (checkWiFiConnection() && !checkMQTTConnection()){
        connectMQTT(); 
    }
    if (checkMQTTConnection()) loopMQTT();

    //Actualizar los datos
    updateSim(datos_simulados);

    // Publish every 5s
    if (millis() - lastPublish >= PUBLISH_INTERVAL_BMS) {
        lastPublish = millis();
        publishTelemetry(datos_simulados);
    }
}
