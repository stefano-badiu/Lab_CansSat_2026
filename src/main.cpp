#include <SoftwareSerial.h>
#include <Arduino.h>
#include <Wire.h>
#include "Sputnik_Identity.hpp"
#include "BMP280.hpp"
#include "MPU6050.hpp"
#include "GPS_PA6H.hpp"
#include "Xbee_S2C.hpp"
//#include "MicroSD.hpp"
#include "MissionControl.hpp"
#include <EEPROM.h>

// --- INTERRUTTORE DI MISSIONE ---
#define MOD_TEST // %%%%%%%%%%%%Commenta questa riga per il lancio reale%%%%%%%%%%%%%

Telemetry current_data;
FSM currentState = STATE_IDLE;
//SERIALI VIRTUALI
SoftwareSerial SerialCamera(7, 8);




void setup() {
 // inizializzazione Hardware Universale
Wire.begin(); 
Wire.setWireTimeout(25000, true);
Serial.begin(9600); 
SerialCamera.begin(9600); 

/*In Arduino, se avvii due SoftwareSerial, lui ascolta solo l'ultima accesa.
Dobbiamo dirgli di ignorare la fotocamera (che tanto deve solo trasmettere)
e di tenere l'orecchio incollato al modulo GPS.*/
focus_GPS();
current_data.TEAM_ID = 33; 
current_data.MISSION_TIME = 0;
current_data.STATE = currentState;
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

    #ifdef MOD_TEST
        // Logica specifica per il TEST (PC)
        while (!Serial); // Aspetta che si accenda il monitor seriale
        Serial.println(F("--- SPUTNIK-33cl: SYSTEM CHECK (TEST) ---"));

        if (!init_GPS()) {Serial.println(F("AVVISO: GPS non risponde."));} 
        else {Serial.println(F("GPS: OK"));}
        
        if (!init_BMP280()) {Serial.println(F("ERRORE CRITICO: BMP280 non trovato!"));} 
        else {Serial.println(F("BMP280: OK"));}

        if(!init_MPU6050()) {Serial.println(F("ERRORE: MPU6050 NON TROVATO"));} 
        else {Serial.println(F("MPU6050: OK"));}

        if (!calibrate_MPU6050_accel(100, 50)) {Serial.println(F("AVVISO: Impossibile calibrare MPU6050 accelerometro"));} 
        else {Serial.println(F("MPU6050: Calibrazione OK"));}
        
        /*if(!init_MicroSD()) {Serial.println(F("ERRORE: MicroSD NON TROVATA"));} 
        else {
            Serial.println(F("MicroSD: OK"));
            if (createLogFile()&& writeLogHeader()) {
                flushLogFile();
                Serial.println(F("Log file pronto"));
            } else {
               Serial.println(F("ERRORE: Impossibile creare file di log"));
            }
        }*/
    
    
        #else
        delay(2000); // Pausa di sicurezza per stabilizzare l'elettronica
        init_mission_control(); 
        init_BMP280(); // Inizializzazione
        init_GPS();
        init_Xbee(); // --- AGGIUNTA XBEE: Inizializzazione seriale radio ---
        FSM stato_salvato;
        EEPROM.get(0, stato_salvato); // Leggiamo il "Disco Nero"
        // Se lo stato non è IDLE, significa che eravamo già in volo e siamo crashati
        if (stato_salvato != STATE_IDLE && stato_salvato <= STATE_LANDED) {
            current_data.STATE = stato_salvato;
            EEPROM.get(4, P_0); // Ripristiniamo la missione!
            // (Opzionale: potremmo voler inviare un segnale via XBee tipo "WARNING: REBOOT OCCURRED")
        } else {
            // Se era IDLE (o la memoria è vuota), è un avvio normale
            current_data.STATE = STATE_IDLE;
            calibration_BMP280();
        }
        
    #endif
}


unsigned long lastTelemetryTime = 0;
unsigned long lastPhotoTime = 0;
void loop() {
    unsigned long inizio_ciclo = millis();
    check_radio_commands();
    accumulate_MPU6050_data(); // raccoglie l'accelerazione a raffica
    read_BMP280();
    update_mission_state(); // Controlla l'altitudine per il paracadute
    // Qui aggiungeremo il resto dei sensori
    if (photoInterval > 0 && (millis() - lastPhotoTime >= photoInterval)) {
        lastPhotoTime = millis(); // Resetta il timer della fotocamera
        
        // Invio diretto senza classe String (Zero frammentazione RAM!)
        // Usiamo la macro F() per salvare anche i punti e virgola nella memoria Flash
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
        current_data.STATE = currentState; // Aggiorna lo stato attuale (da implementare la logica di transizione)
        compute_and_save_MPU6050();
        update_GPS_data(); //sono lenti, li aggiorno solo 1 volta al secondo
    #ifdef MOD_TEST
        // Telemetria Dettagliata per il TEST (PC)
        Serial.print(F("Time: ")); Serial.print(current_data.MISSION_TIME);
        Serial.print(F(" | Alt: ")); Serial.print(current_data.ALTITUDE); Serial.print(F("m"));
        Serial.print(F(" | Lon: ")); Serial.print(current_data.GPS_LONGITUDE, 6);
        Serial.print(F(" | Lat: ")); Serial.print(current_data.GPS_LATITUDE, 6);
        Serial.print(F(" | Sats: ")); Serial.println(current_data.GPS_SATS);
        Serial.print(F(" m | Temp: "));Serial.print(current_data.TEMPERATURE);Serial.println(F(" *C"));
        Serial.print(F("TILT_X: "));Serial.print(current_data.TILT_X);
        Serial.print(F("  TILT_Y: "));Serial.print(current_data.TILT_Y);
        Serial.print(F("  TILT_Z: "));Serial.println(current_data.TILT_Z);
        Serial.print(F("ACC_X: "));Serial.print(current_data.ACC_X);
        Serial.print(F("  ACC_Y: "));Serial.print(current_data.ACC_Y);
        Serial.print(F("  ACC_Z: "));Serial.println(current_data.ACC_Z);
        transmit_telemetry();
    #else
        // Telemetria CSV Compatta per la Ground Station (XBee)
        // Formato: TEAM_ID, MISSION_TIME, STATE, ALTITUDE, TEMP, ... %%%%%%%%DA RIVEDERE%%%%%%%%%%%
       transmit_telemetry(); // --- AGGIUNTA XBEE: Invio effettivo via radio ---
       
    #endif
    /*
    //Salvataggio su SD sempre attivo
        if (is_MicroSD_ready()) {
            if (logTelemetry(current_data)) {
                flushLogFile();
               #ifdef MOD_TEST
                    Serial.println(F("-> Dati salvati su SD."));
                #endif
            }
         }*/
         #ifdef MOD_TEST
             unsigned long tempo_di_esecuzione = millis() - inizio_ciclo; 
             Serial.print(F(">> SFORZO CPU MAX (Sensori + SD) (ms): "));
             Serial.println(tempo_di_esecuzione);
             Serial.println(F("-----------------------------------"));
         #endif
    }
    
}
