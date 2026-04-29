#include "Xbee_S2C.hpp"
#include <Arduino.h>     // Serial, millis
#include "BMP280.hpp"
#include "MissionControl.hpp"



    extern Telemetry current_data; 
    extern bool manual_override;

void init_Xbee() {
    // La porta Seriale (Pin 0 e 1) è già stata aperta nel setup del main a 9600 baud.
    // Qui mandiamo solo un "ping" iniziale per far sapere a terra che la radio è viva.
    Serial.println(F("<SPUTNIK-33cl: SISTEMA RADIO (HARDWARE) ATTIVO>"));
}

void transmit_telemetry() {
    // Costruiamo il pacchetto CSV compatto.
    Serial.print(F("<"));
    Serial.print(current_data.TEAM_ID);          Serial.print(F(","));
    Serial.print(current_data.MISSION_TIME);     Serial.print(F(","));
    Serial.print(current_data.STATE);            Serial.print(F(","));
    Serial.print(current_data.ALTITUDE);         Serial.print(F(","));
    Serial.print(current_data.TEMPERATURE);      Serial.print(F(","));
    Serial.print(current_data.GPS_LATITUDE, 6);  Serial.print(F(","));
    Serial.print(current_data.GPS_LONGITUDE, 6); Serial.print(F(","));
    Serial.print(current_data.GPS_SATS);         Serial.print(F(","));
    Serial.print(current_data.TILT_X);           Serial.print(F(","));
    Serial.print(current_data.TILT_Y);           Serial.print(F(","));
    Serial.print(current_data.TILT_Z);           Serial.print(F(","));
    Serial.print(current_data.ACC_X);            Serial.print(F(","));
    Serial.print(current_data.ACC_Y);            Serial.print(F(","));
    Serial.print(current_data.ACC_Z);            Serial.print(F(","));
    Serial.print(current_data.PARACHUTE_OPEN);   Serial.print(F(","));
    Serial.print(current_data.BATTERY_VOLTAGE_V); Serial.print(F(","));
    Serial.print(current_data.BATTERY_CURRENT_mA); Serial.print(F(","));
    Serial.print(current_data.BATTERY_POWER_mW);   Serial.print(F(","));
    Serial.print(current_data.BATTERY_CONSUMED_mWH); Serial.print(F(","));
    Serial.print(current_data.BATTERY_REMAINING_PCT);
    Serial.println(F(">"));
    

}

void check_radio_commands(){
    if (Serial.available()>0){
        char command = Serial.read();
    switch(command){
            case 'O':
                manual_override = false; 
                break;
            case 'I': 
                change_state(STATE_IDLE);
                manual_override = true; 
                break;
            case 'A':
                change_state(STATE_ASCENT);
                manual_override = true; 
                break;
            case 'S':
                change_state(STATE_DESCENT_SLOW);
                manual_override = true; 
                break;
            case 'L':
                change_state(STATE_LANDED);
                manual_override = true; 
                break;
            case 'C':
            calibration_BMP280(); 
            Serial.println(F("<ACK: CALIBRAZIONE ESEGUITA>"));
            break;
        //case P: take_photo();  potremmo aggiungere questo comando per fare foto quando vogliamo
        default:
            // Ignora qualsiasi carattere che non sia O, I, A, S, L, C
        break;

    }
}
}
