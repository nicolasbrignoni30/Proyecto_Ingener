#include <telemetria.h> 

// Librerías de configuración y credenciales privadas
#include <credentials.h>
#include <config.h>
#include <string>

// Librerías específicas para la conectividad y el formateo de datos
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


WiFiClient         wifiClient;
PubSubClient       mqtt(wifiClient);


bool checkWiFiConnection(){
    return (WiFi.status() == WL_CONNECTED);
}

void connectWiFi() {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500); Serial.print("."); attempts++;
    }
    if (WiFi.status() == WL_CONNECTED)
         Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    else
        Serial.println("\n[WiFi] Failed — continuing without WiFi");
}

bool checkMQTTConnection(){
    return mqtt.connected();
}

void connectMQTT() {
    mqtt.setServer(TB_HOST, TB_PORT);
    mqtt.setBufferSize(2048);
    String clientId = "ESP32_test_tb_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("[MQTT] Connecting...");
    if (mqtt.connect(clientId.c_str(), TB_ACCESS_TOKEN, nullptr)) {
        Serial.println(" OK");
    } else {
        Serial.printf(" failed rc=%d\n", mqtt.state());
    }
}

void loopMQTT(){
    mqtt.loop();
}

//####################################
//Esto se corresponde con el Inversor
//####################################

void publishTelemetryInv(const InvData& inv, const std::string& campo) {
    JsonDocument doc;
    bool campoValido = true;

    if (campo == "StatusData") {
        doc["fault"]     = inv.status.fault;
        doc["alarm"]     = inv.status.alarm;
        doc["running"]   = inv.status.running;
        doc["grid_tied"] = inv.status.grid_tied;
        
    } else if (campo == "AcData") {
        doc["freq_hz"]   = inv.ac.freq_hz;
        doc["v_a"]       = inv.ac.v_a;
        doc["i_a"]       = inv.ac.i_a;
        doc["p_inv_kw"]  = inv.ac.p_inv;
        doc["pf_total"]  = inv.ac.pf_total;
        
    } else if (campo == "DcData") {
        doc["dc_p_kw"]   = inv.dc.power_kw;
        doc["dc_v"]      = inv.dc.voltage_v;
        doc["dc_i"]      = inv.dc.current_a;
        
    } else if (campo == "GridData") {
        doc["grid_freq"] = inv.grid.freq_hz;
        doc["grid_v_a"]  = inv.grid.v_a;
        doc["grid_p_kw"] = inv.grid.p_kw;
        
    } else if (campo == "LoadData") {
#ifdef INVERTER_PROTOCOL_V3
        doc["load_p_kw"] = inv.load.p_total;
        doc["load_s_kva"]= inv.load.s_total;
#endif
    } else if (campo == "FirmData") {
        doc["fw_model"]        = inv.firm.fw_model;
        doc["fw_hw_version"]   = inv.firm.fw_hw_version;
        doc["fw_dsp_version"]  = inv.firm.fw_dsp_version;
        doc["fw_com_version"]  = inv.firm.fw_com_version;
        doc["fw_rtu_protocol"] = inv.firm.fw_rtu_protocol;
    } else {
        Serial.println("[MQTT] Error: Campo de telemetría no reconocido.");
        campoValido = false;
    }

    // Si el campo fue válido y agregamos datos al JSON, serializamos y publicamos
    if (campoValido) {
        char payload[512];
        serializeJson(doc, payload, sizeof(payload));
        mqtt.publish("v1/devices/me/telemetry", payload);
    }
}

//####################################
// Esto se corresponde con el BMS
//####################################

void publishTelemetryBMS(const BmsData& datosBms){
    JsonDocument doc;

    //Estos corresponden al 0x421x  
    doc["bms_voltage_v"] = datosBms.voltage_v;
    doc["bms_current_a"] = datosBms.current_a;
    doc["bms_temp_avg_c"] = datosBms.temp_avg_c;    
    doc["bms_soc_pct"] = datosBms.soc_pct;
    doc["bms_soh_pct"] = datosBms.soh_pct;

    //Estos corresponden al 0x422x
    doc["charge_cutoff_v"] = datosBms.charge_cutoff_v;
    doc["discharge_cutoff_v"] = datosBms.discharge_cutoff_v;
    doc["max_charge_a"] = datosBms.max_charge_a;
    doc["max_discharge_a"] = datosBms.max_discharge_a;

    //Estos corresponden al 0x423x
    doc["cell_voltage_max_v"] = datosBms.cell_voltage_max_v;
    doc["cell_voltage_min_v"] = datosBms.cell_voltage_min_v;

    //Estos corresponden al 0x424x
    doc["temp_cell_max_c"] = datosBms.temp_cell_max_c;
    doc["temp_cell_min_c"] = datosBms.temp_cell_min_c;

    //Estos corresponden al 0x425x
    doc["fault"] = datosBms.fault;
    doc["alarm"] = datosBms.alarm;
    doc["protection"] = datosBms.protection;
    doc["charging"] = datosBms.charging;
    doc["discharging"] = datosBms.discharging;
    doc["force_charge_req"] = datosBms.force_charge_req;

    //Estos correcponden al 00x428x
    doc["soe_pct"] = datosBms.soe_pct;

    //Quedan temp_fet_c, charge_forbidden y discharge_forbidden
    //los ultimos dos los encontre en ex428x pero no son bool sino una mark que es 0xAA.
    //Igualmente lo agrego aca, pero en el .ccp del parseo les agrego valores basura, hay que verlo bien despues.

    doc["charging_forbidden"] = datosBms.charge_forbidden;
    doc["discharging_forbidden"] = datosBms.discharge_forbidden;
    doc["temp_fet_c"] = datosBms.temp_fet_c;

    char payload[2048];
    serializeJson(doc, payload, sizeof(payload));
    Serial.printf("[MQTT] Payload %d bytes\n", strlen(payload));
    bool ok = mqtt.publish("v1/devices/me/telemetry", payload);
}


//#######################################################################
// Las funciones de aca abajo son para datos simulados, se usan en test_tb
// para mandar por telemetria cosas simuladas.
//#######################################################################
float drift(float v, float step, float lo, float hi) {
    v += (random(-100, 100) / 100.0f) * step;
    return constrain(v, lo, hi);
}

void updateSim(Sim& sim) {
    sim.soc            = drift(sim.soc,          0.2f,  10.0f,  95.0f);
    sim.bms_v          = drift(sim.bms_v,         1.0f, 450.0f, 540.0f);
    sim.bms_i          = drift(sim.bms_i,         5.0f,-200.0f, 200.0f);
    sim.bms_temp_avg   = drift(sim.bms_temp_avg,  0.2f,  15.0f,  45.0f);
    sim.bms_temp_max   = sim.bms_temp_avg + 2.0f;
    sim.bms_temp_min   = sim.bms_temp_avg - 2.0f;
    sim.bms_temp_fet   = sim.bms_temp_avg + 4.0f;
    sim.bms_cell_v_max = drift(sim.bms_cell_v_max, 0.005f, 3.0f, 3.65f);
    sim.bms_cell_v_min = sim.bms_cell_v_max - drift(0.03f, 0.002f, 0.01f, 0.08f);
    sim.p_inv          = drift(sim.p_inv,         1.0f,   0.0f,  30.0f);
    sim.grid_p         = drift(sim.grid_p,         1.0f,   0.0f,  50.0f);
    sim.load_p         = drift(sim.load_p,         0.5f,   1.0f,  20.0f);
    sim.freq           = drift(sim.freq,          0.02f,  49.8f,  50.2f);
    sim.v_phase        = drift(sim.v_phase,        0.5f, 220.0f, 240.0f);
    sim.pf             = drift(sim.pf,            0.01f,  0.85f,   1.0f);
}


void publishTelemetrySim(const Sim& sim) {
    JsonDocument doc;

    doc["running"]    = 1;
    doc["fault"]      = 0;
    doc["alarm"]      = 0;
    doc["grid_tied"]  = 1;
    doc["off_grid"]   = 0;
    doc["derating"]   = 0;
    doc["standby"]    = 0;

    doc["freq_hz"]    = sim.freq;
    doc["v_a"]        = sim.v_phase;
    doc["v_b"]        = sim.v_phase - 0.3f;
    doc["v_c"]        = sim.v_phase + 0.2f;
    doc["v_ab"]       = sim.v_phase * 1.732f;
    doc["v_bc"]       = sim.v_phase * 1.732f - 0.5f;
    doc["v_ca"]       = sim.v_phase * 1.732f + 0.3f;
    doc["i_a"]        = sim.p_inv / 3.0f / sim.v_phase * 1000.0f;
    doc["i_b"]        = sim.p_inv / 3.0f / sim.v_phase * 1000.0f - 0.1f;
    doc["i_c"]        = sim.p_inv / 3.0f / sim.v_phase * 1000.0f + 0.1f;
    doc["p_a_kw"]     = sim.p_inv / 3.0f;
    doc["p_b_kw"]     = sim.p_inv / 3.0f - 0.1f;
    doc["p_c_kw"]     = sim.p_inv / 3.0f + 0.1f;
    doc["p_inv_kw"]   = sim.p_inv;
    doc["q_a_kvar"]   = sim.p_inv / 3.0f * 0.1f;
    doc["q_b_kvar"]   = sim.p_inv / 3.0f * 0.1f;
    doc["q_c_kvar"]   = sim.p_inv / 3.0f * 0.1f;
    doc["q_inv_kvar"] = sim.p_inv * 0.1f;
    doc["pf_a"]       = sim.pf;
    doc["pf_b"]       = sim.pf - 0.01f;
    doc["pf_c"]       = sim.pf + 0.01f;
    doc["pf_total"]   = sim.pf;

    doc["dc_voltage_v"] = sim.bms_v;
    doc["dc_current_a"] = sim.p_inv / sim.bms_v * 1000.0f;
    doc["dc_power_kw"]  = sim.p_inv;

    doc["grid_freq_hz"] = sim.freq;
    doc["grid_v_a"]     = sim.v_phase;
    doc["grid_v_b"]     = sim.v_phase - 0.3f;
    doc["grid_v_c"]     = sim.v_phase + 0.2f;
    doc["grid_p_kw"]    = sim.grid_p;

    doc["load_freq_hz"] = sim.freq;
    doc["load_v_a"]     = sim.v_phase;
    doc["load_v_b"]     = sim.v_phase - 0.3f;
    doc["load_v_c"]     = sim.v_phase + 0.2f;
    doc["load_i_a"]     = sim.load_p / 3.0f / sim.v_phase * 1000.0f;
    doc["load_i_b"]     = sim.load_p / 3.0f / sim.v_phase * 1000.0f;
    doc["load_i_c"]     = sim.load_p / 3.0f / sim.v_phase * 1000.0f;
    doc["load_p_a_kw"]  = sim.load_p / 3.0f;
    doc["load_p_b_kw"]  = sim.load_p / 3.0f;
    doc["load_p_c_kw"]  = sim.load_p / 3.0f;
    doc["load_p_kw"]    = sim.load_p;
    doc["load_s_kva"]   = sim.load_p / sim.pf;

    doc["bms_soc_pct"]             = sim.soc;
    doc["bms_soh_pct"]             = 98.0f;
    doc["bms_voltage_v"]           = sim.bms_v;
    doc["bms_current_a"]           = sim.bms_i;
    doc["bms_temp_avg_c"]          = sim.bms_temp_avg;
    doc["bms_temp_cell_max_c"]     = sim.bms_temp_max;
    doc["bms_temp_cell_min_c"]     = sim.bms_temp_min;
    doc["bms_temp_fet_c"]          = sim.bms_temp_fet;
    doc["bms_cell_v_max"]          = sim.bms_cell_v_max;
    doc["bms_cell_v_min"]          = sim.bms_cell_v_min;
    doc["bms_max_charge_a"]        = sim.bms_max_chg_a;
    doc["bms_max_discharge_a"]     = sim.bms_max_dischg_a;
    doc["bms_charge_cutoff_v"]     = sim.bms_chg_cutoff;
    doc["bms_discharge_cutoff_v"]  = sim.bms_dischg_cutoff;
    doc["bms_charging"]            = sim.bms_i > 0 ? 1 : 0;
    doc["bms_discharging"]         = sim.bms_i < 0 ? 1 : 0;
    doc["bms_charge_forbidden"]    = 0;
    doc["bms_discharge_forbidden"] = 0;
    doc["bms_force_charge"]        = 0;
    doc["bms_fault"]               = 0;
    doc["bms_alarm"]               = 0;
    doc["bms_protection"]          = 0;

    char payload[2048];
    serializeJson(doc, payload, sizeof(payload));
    Serial.printf("[MQTT] Payload %d bytes\n", strlen(payload));
    bool ok = mqtt.publish("v1/devices/me/telemetry", payload);
    //mqttOk = ok || mqtt.connected();
    Serial.printf("[MQTT] Publish %s — SOC=%d%% P_inv=%.1fkW Grid=%.1fkW\n",
                  ok ? "OK" : "FAIL", (int)sim.soc, sim.p_inv, sim.grid_p);
}