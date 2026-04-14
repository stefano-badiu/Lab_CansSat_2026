#include "MissionControl.hpp"
#include <Arduino.h>

unsigned long photoInterval = 0;


//void init_mission_control() {} //ci lavorerò quando penserò alla lettura su SD card

void change_state(FSM state){
    current_data.STATE= state;
        //&&&&&&&&&&&&&&&&&stampa seriale per test&&&&&&&&&&&&&&&&&&&
        Serial.print("MISSION UPDATE: Passaggio allo stato ");
        Serial.println(state); 
    switch(state) {
        case STATE_IDLE:          photoInterval = 0;     break; // Ferma
        case STATE_ASCENT:        photoInterval = 10000; break; // 10 sec
        case STATE_DESCENT_FAST:  photoInterval = 2000;  break; // 2 sec
        case STATE_DESCENT_SLOW:  photoInterval = 1000;  break; // 1 sec
        case STATE_LANDED:        photoInterval = 0;     break; // Ferma
    }    


}


void update_mission_state(){
    switch(current_data.STATE) { 
        case STATE_IDLE:
        //azioni che dovrà eseguire in questo stato
            if(detect_launch()){
                change_state(STATE_ASCENT);
            }
        break;

        case STATE_ASCENT:
        //Azioni
            if(detect_apogee()){
                change_state(STATE_DESCENT_FAST);
            }
        break;

        case STATE_DESCENT_FAST:
        //azioni
            if(detect_altitude()){
                change_state(STATE_DESCENT_SLOW);
            }
        break;

        case STATE_DESCENT_SLOW:
        //azioni
            if(detect_landing()){
                change_state(STATE_LANDED);
            }
        break;

        case STATE_LANDED:
        //azioni

    }

}