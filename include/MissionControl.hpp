#pragma once
#include "Sputnik_Identity.hpp"

const int PIN_SERVO = 2; 
//const int PIN_BEACON = 3; //serve per sentire il cansat una volta atterrati, a quanto pare il gps potrebbe non funzionare più a terra




extern Telemetry current_data;
extern bool manual_override;
extern unsigned long photoInterval;


void init_mission_control();  //controlla se la missione è iniziata
void update_mission_state();
bool detect_launch();
bool detect_apogee();
bool detect_altitude();
bool detect_landing();
void change_state(FSM state); //la funzione accetta solo parametri di tipo FSM, state è stato usato come parametro globale per l'inizializzazione
