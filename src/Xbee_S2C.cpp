#include "Xbee_S2C.hpp"
#include <Arduino.h>     // Serial, millis
#include "BMP280.hpp"
#include "MPU6050.hpp"
#include "GPS_PA6H.hpp"
#include "INA219.hpp"
#include "Sputnik_Identity.hpp"
#include "MissionStorage.hpp"
#include "MissionControl.hpp"
#include <Servo.h>



    extern Telemetry current_data; 
    extern bool manual_override;
    extern INA219BatteryMonitor batteryMonitor;

namespace {
bool is_ground_state() {
    return current_data.STATE == STATE_IDLE || current_data.STATE == STATE_LANDED;
}

bool looks_airborne() {
    return bmp_ok &&
           (current_data.ALTITUDE > 20.0F ||
            current_data.VERTICAL_SPEED < -2.0F);
}

void deny_in_flight() {
    Serial.println(F("<ERRORE: COMANDO NEGATO IN VOLO>"));
}

void command_recovery_ascent() {
    if (current_data.STATE != STATE_IDLE) {
        Serial.println(F("<ERRORE: ASCENT NEGATO>"));
        return;
    }

    reset_mission_detectors();
    change_state(STATE_ASCENT);
    manual_override = false;
    Serial.println(F("<ACK: RECOVERY ASCENT, FSM AUTO>"));
}

void command_deploy_parachute() {
    if (current_data.PARACHUTE_OPEN) {
        Serial.println(F("<ACK: PARACADUTE GIA APERTO>"));
        return;
    }

    if (is_ground_state() && !looks_airborne()) {
        apri_paracadute();
        Serial.println(F("<ACK: PARACADUTE APERTO TEST>"));
        return;
    }

    if (current_data.STATE != STATE_ASCENT &&
        current_data.STATE != STATE_DESCENT_FAST &&
        current_data.STATE != STATE_IDLE) {
        Serial.println(F("<ERRORE: DEPLOY NEGATO>"));
        return;
    }

    reset_mission_detectors();
    change_state(STATE_DESCENT_SLOW);
    manual_override = false;
    Serial.println(F("<ACK: DEPLOY EMERGENZA, FSM AUTO>"));
}
}


    

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
    Serial.print(current_data.BATTERY_REMAINING_PCT);Serial.print(F(","));
    Serial.print(current_data.PRESSURE);       
    Serial.println(F(">"));
    

}

void check_radio_commands() {
    static char buffer[15];
    static uint8_t index = 0;
    static bool is_reading = false;
    static unsigned long packet_start_ms = 0;

    const unsigned long PACKET_TIMEOUT_MS = 200;

    if (is_reading && (millis() - packet_start_ms > PACKET_TIMEOUT_MS)) {
        is_reading = false;
        index = 0;
    }

    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '<') {
            is_reading = true;
            index = 0;
            packet_start_ms = millis();
        }
        else if (c == '>' && is_reading) {
            buffer[index] = '\0';
            is_reading = false;

            if (index == 5 &&
                buffer[0] == 'C' &&
                buffer[1] == 'M' &&
                buffer[2] == 'D' &&
                buffer[3] == ':') {

                char comando_reale = buffer[4];

                switch (comando_reale) {
                    
                    case 'R':
                            if (is_ground_state()) {
                            clear_saved_mission();
                            change_state(STATE_IDLE);
                            if (bmp_ok) {
                             calibration_BMP280();
                            }
                            calibrate_MPU6050_accel(100, 10);
                            batteryMonitor.resetEnergyCounter();
                            reset_mission_detectors();
                            manual_override = false;
                            Serial.println(F("<ACK: RESET GENERALE E CALIBRAZIONE COMPLETATI. SISTEMA ARMATO>"));
                            } else {
                            deny_in_flight();
                            }
                            break;

                    case 'C':
                            if (is_ground_state()) {
                                if (bmp_ok) {
                                    calibration_BMP280();
                                    reset_mission_detectors();
                                    Serial.println(F("<ACK: CALIBRAZIONE BAROMETRO ESEGUITA>"));
                                }
                            } else {
                                deny_in_flight();
                            }
                            break;

                    case 'M':
                            if (is_ground_state()) {
                            calibrate_MPU6050_accel(100, 10);
                            Serial.println(F("<ACK: CALIBRAZIONE INERZIALE ESEGUITA>"));
                            } else {
                            deny_in_flight();
                            }
                            break;

                    case '0':
                            if (is_ground_state()) {
                            chiudi_paracadute();
                            Serial.println(F("<ACK: PARACADUTE CHIUSO>"));
                            } else {
                            deny_in_flight();
                            }
                            break;

                    case '1':
                            command_deploy_parachute();
                            break;

                    case 'O':
                            manual_override = false;
                            reset_mission_detectors();
                            Serial.println(F("<ACK: FSM AUTOMATICA ATTIVA>"));
                            break;

                    case 'I':
                            if (is_ground_state()) {
                            reset_mission_detectors();
                            change_state(STATE_IDLE);
                            manual_override = false;
                            Serial.println(F("<ACK: STATO IDLE, FSM AUTO>"));
                            } else {
                            deny_in_flight();
                            }
                            break;

                    case 'A':
                            command_recovery_ascent();
                            break;

                    case 'S':
                            command_deploy_parachute();
                            break;

                    case 'L':
                            if (is_ground_state()) {
                            reset_mission_detectors();
                            change_state(STATE_LANDED);
                            manual_override = false;
                            Serial.println(F("<ACK: STATO LANDED, FSM AUTO>"));
                            } else {
                            deny_in_flight();
                            }
                            break;
                           
                    default:
                        Serial.println(F("<ERRORE: COMANDO SCONOSCIUTO>"));
                        break;
                } 
            } else {
                // Se la firma non è CMD: o la lunghezza è sbagliata
                Serial.println(F("<ERRORE: FIRMA PACCHETTO NON VALIDA>"));
            } // Chiude l'if sulla validità del pacchetto

            index = 0; // Prepara l'indice per il prossimo pacchetto

        } 
        
        else if (is_reading) {
            // Se stiamo leggendo il contenuto del pacchetto (tra < e >)
            if (index < sizeof(buffer) - 1) {
                buffer[index++] = c;
            } else {
                // Buffer overflow
                is_reading = false;
                index = 0;
                Serial.println(F("<ERRORE: PACCHETTO RADIO TROPPO LUNGO>"));
            }
        } 
        
    } 
}


/*case 'R':if (current_data.STATE == STATE_IDLE || current_data.STATE == STATE_LANDED) {
                            clear_saved_mission();
                            change_state(STATE_IDLE);
                            calibration_BMP280();
                            calibrate_MPU6050_accel(100, 10);
                            batteryMonitor.resetEnergyCounter();
                            Serial.println(F("<ACK: RESET GENERALE E CALIBRAZIONE COMPLETATI. SISTEMA ARMATO>"));
                        } else {
                            Serial.println(F("<ERRORE: COMANDO NEGATO IN VOLO>"));
                        }
                        break;
                    case 'C': if(current_data.STATE == STATE_IDLE || current_data.STATE == STATE_LANDED) {
                         calibration_BMP280();
                         Serial.println(F("<ACK: CALIBRAZIONE BAROMETRO ESEGUITA>"));
                        }else{
                        Serial.println(F("<ERRORE: COMANDO NEGATO IN VOLO>"));
                        }
                        break;

                    case 'M': if(current_data.STATE == STATE_IDLE || current_data.STATE == STATE_LANDED) {
                        calibrate_MPU6050_accel(100, 10);
                        Serial.println(F("<ACK: CALIBRAZIONE INERZIALE ESEGUITA>"));
                        break;
                    } else {
                        Serial.println(F("<ERRORE: COMANDO NEGATO IN VOLO>"));
                    }
                    break;
                    case '0':
                        chiudi_paracadute();
                        Serial.println(F("<ACK: PARACADUTE CHIUSO>"));
                        break;

                    case '1':
                        apri_paracadute();
                        Serial.println(F("<ACK: PARACADUTE APERTO>"));
                        break;

                    case 'O':
                        manual_override = false;
                        Serial.println(F("<ACK: OVERRIDE MANUALE DISATTIVATO>"));
                        break;

                    case 'I':
                        change_state(STATE_IDLE);
                        manual_override = true;
                        Serial.println(F("<ACK: STATO FORZATO IDLE>"));
                        break;

                    case 'A':
                        change_state(STATE_ASCENT);
                        manual_override = true;
                        Serial.println(F("<ACK: STATO FORZATO ASCENT>"));
                        break;

                    case 'S':
                        change_state(STATE_DESCENT_SLOW);
                        manual_override = true;
                        Serial.println(F("<ACK: STATO FORZATO DESCENT_SLOW>"));
                        break;

                    case 'L':
                        change_state(STATE_LANDED);
                        manual_override = true;
                        Serial.println(F("<ACK: STATO FORZATO LANDED>"));
                        break;

                    default:
                        Serial.println(F("<ERRORE: COMANDO SCONOSCIUTO>"));
                        break;*/