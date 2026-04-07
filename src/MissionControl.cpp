#include "MissionControl.hpp"
#include <Arduino.h>
//void init_mission_control() {} //ci lavorerò quando penserò alla lettura su SD card

void change_state(FSM state){
    current_data.STATE= state;
        //&&&&&&&&&&&&&&&&&stampa seriale per test&&&&&&&&&&&&&&&&&&&
        Serial.print("MISSION UPDATE: Passaggio allo stato ");
        Serial.println(state); 


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