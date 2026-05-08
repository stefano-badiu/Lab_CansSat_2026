#pragma once
#include "Sputnik_Identity.hpp"

struct SavedMission {
    bool valid;      // missione/stato FSM valido
    bool p0_valid;   // pressione P0 valida
    FSM state;
    float p0;
};

SavedMission load_saved_mission();
void save_mission_state(FSM state);
void save_ground_pressure(float p0);
void clear_saved_mission();
