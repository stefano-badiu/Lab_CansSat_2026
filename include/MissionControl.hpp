#pragma once
#include "Sputnik_Identity.hpp"
#include <Servo.h>

const int PIN_SERVO_0 = 2;
const int PIN_SERVO_1 = 5;

extern Servo servo_0;
extern Servo servo_1;

void apri_paracadute();
void chiudi_paracadute();

extern Telemetry current_data;
extern bool manual_override;
extern unsigned long photoInterval;
extern bool bmp_ok;

void init_mission_control();
void update_mission_state();
bool detect_launch();
bool detect_release();
bool detect_parachute_deployment();
bool detect_landing();
void change_state(FSM state);
void reset_mission_detectors();
