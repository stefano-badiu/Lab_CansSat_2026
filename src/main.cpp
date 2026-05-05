#include <SoftwareSerial.h>
#include <Arduino.h>
#include <Wire.h>
#include "Sputnik_Identity.hpp"
#include "BMP280.hpp"
#include "MPU6050.hpp"
#include "GPS_PA6H.hpp"
#include "Xbee_S2C.hpp"
#include "MissionControl.hpp"
#include "MissionStorage.hpp"
#include <EEPROM.h>
#include "INA219.hpp"
#include <math.h>



INA219BatteryMonitor batteryMonitor;
Telemetry current_data;
//SERIALI VIRTUALI
SoftwareSerial SerialCamera(7, 8);




void setup() {
 // inizializzazione Hardware Universale
Wire.begin(); 
Wire.setWireTimeout(25000, true);
Serial.begin(9600); 
SerialCamera.begin(19200); 

/*In Arduino, se avvii due SoftwareSerial, lui ascolta solo l'ultima accesa.
Dobbiamo dirgli di ignorare la fotocamera (che tanto deve solo trasmettere)
e di tenere l'orecchio incollato al modulo GPS.*/
focus_GPS();
current_data.TEAM_ID = 33; 
current_data.MISSION_TIME = 0;
current_data.STATE = STATE_IDLE;
current_data.ALTITUDE = 0;
current_data.TEMPERATURE = 0;
current_data.GPS_LATITUDE = 0;
current_data.GPS_LONGITUDE = 0;
current_data.GPS_SATS = 0;
current_data.TILT_X = 0;
current_data.TILT_Y = 0;
current_data.TILT_Z = 0;
current_data.ACC_X = 0;
current_data.ACC_Y = 0;
current_data.ACC_Z = 0;
current_data.PARACHUTE_OPEN = false;
current_data.BATTERY_VOLTAGE_V = 0;
current_data.BATTERY_CURRENT_mA = 0;
current_data.BATTERY_POWER_mW = 0;
current_data.BATTERY_CONSUMED_mWH = 0;
current_data.BATTERY_REMAINING_PCT = 0;
delay(2000); // Pausa di sicurezza per stabilizzare l'elettronica
init_mission_control(); // Controlla se la missione è iniziata (lancio rilevato)
bool bmp_ok = init_BMP280();

if (bmp_ok) {
    calibration_BMP280();
} else {
    // Logga l'errore via XBee e continua
    Serial.println(F("<ERRORE: BMP280 assente. FSM disabilitata>"));
}
        init_GPS();
        init_Xbee();
        batteryMonitor.init_INA219();
        batteryMonitor.setBatteryCapacity(1000.0f); // stessa capacità del test
        batteryMonitor.setInitialSocPercent(100.0f);
        init_MPU6050(); // --- AGGIUNTA XBEE: Inizializzazione seriale radio ---
        SavedMission saved = load_saved_mission();
        if (saved.valid) {
            P_0 = saved.p0;
            change_state(saved.state);
        } else {
            current_data.STATE = STATE_IDLE;
            clear_saved_mission();
            calibration_BMP280();
        }
}

unsigned long lastTelemetryTime = 0;
unsigned long lastPhotoTime = 0;
void loop() {
    static unsigned long lastBMPTime = 0;
    const unsigned long intervallo_BMP = 50; // 50ms = 20 letture al secondo
    unsigned long inizio_ciclo = millis();
    unsigned long tempo_da_telemetria = millis() - lastTelemetryTime;

    check_radio_commands();
    accumulate_MPU6050_data(); // raccoglie l'accelerazione a raffica
    update_GPS_data();
    // 3. Il barometro e la FSM vengono aggiornati solo ogni 50ms
    if (millis() - lastBMPTime >= intervallo_BMP) {
        lastBMPTime = millis();
        
        read_BMP280(); // Ora il calcolo pesante pow() viene eseguito "solo" 20 volte/s
        update_mission_state(); // La FSM valuta la nuova altitudine calcolata
    }

    // Qui aggiungeremo il resto dei sensori
    if (photoInterval > 0 && (millis() - lastPhotoTime >= photoInterval) && (tempo_da_telemetria > 400 && tempo_da_telemetria < 600)) {
        lastPhotoTime = millis(); // Resetta il timer della fotocamera
        // Invio diretto senza classe String (Zero frammentazione RAM!)
        // Usiamo la macro F() per salvare anche i punti e virgola nella memoria Flash
        SerialCamera.print(F("P;"));
        SerialCamera.print(current_data.MISSION_TIME);
        SerialCamera.print(F(";"));
        SerialCamera.print(current_data.ALTITUDE);
        SerialCamera.print(F(";"));
        SerialCamera.println(current_data.STATE); // println chiude il pacchetto con \n
    }
   
    // CICLO DI INVIO DATI OGNI SECONDO (1000ms)
    if (millis() - lastTelemetryTime >= 1000) {
        lastTelemetryTime = millis(); // Resetta il timer
        current_data.MISSION_TIME = millis(); // Aggiorna il tempo di missione
        compute_and_save_MPU6050();
    
        
        batteryMonitor.read_INA219();
        InaSample batterySample = batteryMonitor.getLastSample();
        current_data.BATTERY_VOLTAGE_V = batterySample.voltage_V;
        current_data.BATTERY_CURRENT_mA = batterySample.current_mA;
        current_data.BATTERY_POWER_mW = batterySample.power_mW;
        current_data.BATTERY_CONSUMED_mWH = batterySample.energy_mWh;
        current_data.BATTERY_REMAINING_PCT = batterySample.remaining_pct;

    SerialCamera.print(F("<"));
        SerialCamera.print(current_data.TEAM_ID);              SerialCamera.print(F(","));
        SerialCamera.print(current_data.MISSION_TIME);         SerialCamera.print(F(","));
        SerialCamera.print(current_data.STATE);                SerialCamera.print(F(","));
        SerialCamera.print(current_data.ALTITUDE);             SerialCamera.print(F(","));
        SerialCamera.print(current_data.PRESSURE);             SerialCamera.print(F(",")); 
        SerialCamera.print(current_data.TEMPERATURE);          SerialCamera.print(F(","));
        SerialCamera.print(current_data.GPS_LATITUDE, 6);      SerialCamera.print(F(","));
        SerialCamera.print(current_data.GPS_LONGITUDE, 6);     SerialCamera.print(F(","));
        SerialCamera.print(current_data.GPS_SATS);             SerialCamera.print(F(","));
        SerialCamera.print(current_data.TILT_X);               SerialCamera.print(F(","));
        SerialCamera.print(current_data.TILT_Y);               SerialCamera.print(F(","));
        SerialCamera.print(current_data.TILT_Z);               SerialCamera.print(F(","));
        SerialCamera.print(current_data.ACC_X);                SerialCamera.print(F(","));
        SerialCamera.print(current_data.ACC_Y);                SerialCamera.print(F(","));
        SerialCamera.print(current_data.ACC_Z);                SerialCamera.print(F(","));
        SerialCamera.print(current_data.PARACHUTE_OPEN);       SerialCamera.print(F(","));
        SerialCamera.print(current_data.BATTERY_VOLTAGE_V);    SerialCamera.print(F(","));
        SerialCamera.print(current_data.BATTERY_CURRENT_mA);   SerialCamera.print(F(","));
        SerialCamera.print(current_data.BATTERY_POWER_mW);     SerialCamera.print(F(","));
        SerialCamera.print(current_data.BATTERY_CONSUMED_mWH); SerialCamera.print(F(","));
        SerialCamera.print(current_data.BATTERY_REMAINING_PCT);
        SerialCamera.println(F(">"));
   
// Telemetria CSV Compatta per la Ground Station (XBee)
        // Formato: TEAM_ID, MISSION_TIME, STATE, ALTITUDE, TEMP, ... %%%%%%%%DA RIVEDERE%%%%%%%%%%%
        transmit_telemetry(); // --- AGGIUNTA XBEE: Invio effettivo via radio ---
        
unsigned long tempo_di_esecuzione = millis() - inizio_ciclo; 
        Serial.print(F(">> SFORZO CPU MAX (Sensori + SD) (ms): "));
        Serial.println(tempo_di_esecuzione);    
            }
}
