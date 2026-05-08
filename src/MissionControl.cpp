#include "MissionControl.hpp"
#include <Arduino.h>
#include <MissionStorage.hpp>
#include <Servo.h>
#include <math.h>
#include "BMP280.hpp"

bool manual_override = false;
bool bmp_ok = false; // Variabile globale per indicare se il BMP280 è operativo

unsigned long photoInterval = 0;
unsigned long launch_start_ms = 0;
unsigned long release_start_ms = 0;
unsigned long parachute_start_ms = 0;
unsigned long landing_start_ms = 0;
float landing_reference_altitude = 0.0F;


const int SERVO_0_OPEN = 90;
const int SERVO_0_CLOSED = 0;

const int SERVO_1_OPEN = 0;
const int SERVO_1_CLOSED = 90;

const float LAUNCH_ALTITUDE_M = 10.0;
const float LAUNCH_MIN_VSPEED = +0.3;
const unsigned long LAUNCH_CONFIRMATION_MS = 2000;

const float RELEASE_MIN_ALTITUDE_M = 80.0;
const float RELEASE_MIN_DESCENT_SPEED = -3.0;
const float RELEASE_MIN_MAX_ALTITUDE_M = 115.0F;


const float PARACHUTE_DEPLOY_ALTITUDE_M = 118.0;
const float PARACHUTE_MIN_DESCENT_SPEED = -4.0;

const float LANDED_MAX_ALTITUDE_M = 20.0;
const float LANDED_MAX_ABS_VSPEED = 0.3;
const float LANDED_MAX_ALT_DRIFT = 1.0;


const unsigned long t_conferma_release = 1000; // 1 secondo continuo di caduta

const unsigned long t_conferma_parachute = 300;

const unsigned long t_conferma = 5000; // 5 secondi 

Servo servo_0;
Servo servo_1;

void apri_paracadute() {
    servo_0.write(SERVO_0_OPEN);
    servo_1.write(SERVO_1_OPEN);
    current_data.PARACHUTE_OPEN = true;
}

void chiudi_paracadute() {
    servo_0.write(SERVO_0_CLOSED);
    servo_1.write(SERVO_1_CLOSED);
    current_data.PARACHUTE_OPEN = false;
}

void reset_mission_detectors() {
    launch_start_ms = 0;
    release_start_ms = 0;
    parachute_start_ms = 0;
    landing_start_ms = 0;
    landing_reference_altitude = current_data.ALTITUDE;
}


bool detect_launch() {
    bool quota_ok = current_data.ALTITUDE > LAUNCH_ALTITUDE_M;
    bool salita_ok = current_data.VERTICAL_SPEED > LAUNCH_MIN_VSPEED;

    if (quota_ok && salita_ok) {
        if (launch_start_ms == 0) {
            launch_start_ms = millis(); // Facciamo partire il cronometro
        } 
        // Se rimaniamo sopra i 10 metri per 2 secondi continui, siamo sicuramente in volo
        else if (millis() - launch_start_ms >= LAUNCH_CONFIRMATION_MS) {
            return true; // Passa a STATE_ASCENT
        }
    } else {
        // Se era solo un falso allarme o il drone è riatterrato subito, azzera tutto
        launch_start_ms = 0;
    }
    
    return false;
}

bool detect_release() {
    bool quota_ok = current_data.ALTITUDE > RELEASE_MIN_ALTITUDE_M;
    bool discesa_ok = current_data.VERTICAL_SPEED < RELEASE_MIN_DESCENT_SPEED;
    bool quota_massima_ok = h_max > RELEASE_MIN_MAX_ALTITUDE_M;

    if (quota_ok && discesa_ok && quota_massima_ok) {  

        if (release_start_ms == 0) { 
            release_start_ms = millis(); // Avvia timer
        }
        else if (millis() - release_start_ms >= t_conferma_release) { 
            return true; // RILASCIO CONFERMATO
        }
    } else {
        // Se la quota è tornata su, era un falso allarme: resetta il timer
        release_start_ms = 0; 
    }
    
    return false; 
}

// Aspettiamo 300ms. A 30 m/s, il CanSat perderà circa 9 metri prima dell'apertura.
// Aprirà quindi intorno ai 100 metri di quota, lasciando un margine di sicurezza per compensare eventuali errori di misura o variazioni nelle condizioni di lancio.
bool detect_parachute_deployment() {
    bool sotto_quota = current_data.ALTITUDE <= PARACHUTE_DEPLOY_ALTITUDE_M;
    bool discesa_ok = current_data.VERTICAL_SPEED < PARACHUTE_MIN_DESCENT_SPEED;
    bool paracadute_chiuso = !current_data.PARACHUTE_OPEN;
    bool sotto_massimo = current_data.ALTITUDE < (h_max - 10.0F);


    if (sotto_quota && discesa_ok && paracadute_chiuso && sotto_massimo) {
        if (parachute_start_ms == 0) {
            parachute_start_ms = millis(); // Start timer
        }
        
        if (millis() - parachute_start_ms >= t_conferma_parachute) {
            return true; // OK: Time passed, confirm deploy
        }
    } 
    else {
        // RESET: Solo se la quota è tornata sopra la soglia
        parachute_start_ms = 0;
    }
    return false;   
}


bool detect_landing() {
    bool velocita_ok = fabs(current_data.VERTICAL_SPEED) < LANDED_MAX_ABS_VSPEED;
    bool quota_bassa = current_data.ALTITUDE <= LANDED_MAX_ALTITUDE_M;
    bool quota_stabile = fabs(current_data.ALTITUDE - landing_reference_altitude) < LANDED_MAX_ALT_DRIFT;

    if ((quota_bassa || quota_stabile) && velocita_ok) {
        // Se è la prima volta che lo vediamo fermo, facciamo partire il cronometro
        if (landing_start_ms == 0) {
            landing_start_ms = millis();
        }
        
        // Se è rimasto fermo per più di 5 secondi...
        if (millis() - landing_start_ms >= t_conferma) {
            return true; // ATTERRAGGIO CONFERMATO
        }
    } 
    else {
        // Se si muove, resetta tutto: il timer e la nuova altitudine di riferimento
        landing_start_ms = 0;
        landing_reference_altitude = current_data.ALTITUDE;
    }

    return false;
}


void init_mission_control() {
     servo_0.attach(PIN_SERVO_0);
    servo_1.attach(PIN_SERVO_1);
    chiudi_paracadute();
} 

void change_state(FSM state){
    if (current_data.STATE == state) return;
    current_data.STATE = state;
    save_mission_state(state);

        //&&&&&&&&&&&&&&&&&stampa seriale per test&&&&&&&&&&&&&&&&&&&
        Serial.print(F("MISSION UPDATE: Passaggio allo stato "));
        Serial.println(state); 
    switch(state) {
        case STATE_IDLE:          photoInterval = 0;     break; // Ferma
        case STATE_ASCENT:        photoInterval = 5000; break; // 5 sec
        case STATE_DESCENT_FAST:  photoInterval = 2000;  break; // 2 sec
        case STATE_DESCENT_SLOW:  photoInterval = 2000; apri_paracadute(); break;
        case STATE_LANDED:        photoInterval = 0;     break; // Ferma
    }    


}


void update_mission_state(){
   if (manual_override) return;
    if (!bmp_ok) return; // FSM congelata, ma il resto del sistema gira

    if ((current_data.STATE == STATE_ASCENT || current_data.STATE == STATE_DESCENT_FAST) && !current_data.PARACHUTE_OPEN) {
        bool volo_confermato = (current_data.STATE > STATE_IDLE) || (h_max > 50.0F);
        if (volo_confermato) {
            float quota_controllo = current_data.ALTITUDE; 
            if (current_data.STATE == STATE_DESCENT_FAST) { // calcolo predittivo attivo SOLO in caduta libera
                quota_controllo += (current_data.VERTICAL_SPEED * 0.20F); // Compensa i 200ms di ritardo del filtro
            }
    if (quota_controllo <= 110.0F && current_data.VERTICAL_SPEED <= -8.0F) {
        change_state(STATE_DESCENT_SLOW);
        return;
    }
    }
    }

    switch(current_data.STATE) { 
        case STATE_IDLE:
        //azioni che dovrà eseguire in questo stato
            if(detect_launch()){
                change_state(STATE_ASCENT);
            }
        break;

        case STATE_ASCENT:
        //Azioni
            if(detect_release()){
                change_state(STATE_DESCENT_FAST);
            
            }
        break;

        case STATE_DESCENT_FAST:
        //azioni
            if(detect_parachute_deployment()){
                change_state(STATE_DESCENT_SLOW);
            }
        break;

        case STATE_DESCENT_SLOW:
            if(detect_landing()){
                change_state(STATE_LANDED);
            }
        break;

        case STATE_LANDED:
        //azioni
        break;
    }

}

