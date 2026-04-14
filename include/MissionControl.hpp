#pragma once
#include "Sputnik_Identity.hpp"
extern Telemetry current_data;
extern unsigned long photoInterval;
void init_mission_control();  //controlla se la missione è iniziata
void update_mission_state();
bool detect_launch();
bool detect_apogee();
bool detect_altitude();
bool detect_landing();
void change_state(FSM state); //la funzione accetta solo parametri di tipo FSM, state è stato usato come parametro globale per l'inizializzazione
