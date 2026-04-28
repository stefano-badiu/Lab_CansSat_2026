#pragma once
#include "Sputnik_Identity.hpp"
#include <Arduino.h>



void init_Xbee();
void transmit_telemetry();
void check_radio_commands();
