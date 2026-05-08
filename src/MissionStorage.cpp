#include "MissionStorage.hpp"
#include <EEPROM.h>
#include <math.h>
#include "Sputnik_Identity.hpp"

// --- MAPPA DELLA EEPROM ---
// Indirizzo 0: byte di firma per missione/stato FSM valido
// Indirizzo 1: stato FSM (uint8_t, 1 byte)
// Indirizzo 2: byte di firma per pressione P0 valida
// Indirizzo 3-6: pressione P0 (float, 4 byte)
constexpr int ADDR_MISSION_MAGIC = 0;
constexpr int ADDR_STATE = 1;
constexpr int ADDR_P0_MAGIC = 2;
constexpr int ADDR_PRESSURE = 3;

constexpr uint8_t MAGIC_BYTE = 0xAB;

static bool is_valid_saved_state(FSM state) {
    uint8_t raw = static_cast<uint8_t>(state);
    return raw >= static_cast<uint8_t>(STATE_IDLE) &&
           raw <= static_cast<uint8_t>(STATE_LANDED);
}

static bool is_valid_saved_pressure(float p0) {
    return !isnan(p0) && !isinf(p0) && p0 >= 300.0F && p0 <= 1100.0F;
}

SavedMission load_saved_mission() {
    SavedMission result;
    result.valid = false;
    result.p0_valid = false;
    result.state = STATE_IDLE;
    result.p0 = 1013.25F;

    uint8_t p0_magic = EEPROM.read(ADDR_P0_MAGIC);
    if (p0_magic == MAGIC_BYTE) {
        float saved_p0;
        EEPROM.get(ADDR_PRESSURE, saved_p0);

        if (is_valid_saved_pressure(saved_p0)) {
            result.p0_valid = true;
            result.p0 = saved_p0;
        }
    }

    uint8_t mission_magic = EEPROM.read(ADDR_MISSION_MAGIC);
    if (mission_magic == MAGIC_BYTE) {
        uint8_t raw_state;
        EEPROM.get(ADDR_STATE, raw_state);

        FSM saved_state = static_cast<FSM>(raw_state);

        if (is_valid_saved_state(saved_state)) {
            result.valid = true;
            result.state = saved_state;
        }
    }

    return result;
}

void save_mission_state(FSM state) {
    EEPROM.write(ADDR_MISSION_MAGIC, MAGIC_BYTE);
    EEPROM.put(ADDR_STATE, static_cast<uint8_t>(state));
}

void save_ground_pressure(float p0) {
    EEPROM.write(ADDR_P0_MAGIC, MAGIC_BYTE);
    EEPROM.put(ADDR_PRESSURE, p0);
}

void clear_saved_mission() {
    EEPROM.write(ADDR_MISSION_MAGIC, 0xFF);
}
