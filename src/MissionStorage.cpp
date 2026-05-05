#include "MissionStorage.hpp"
#include <EEPROM.h>
#include <math.h>
#include "Sputnik_Identity.hpp"

// --- MAPPA DELLA EEPROM ---
// Indirizzo 0: byte di firma (0xAB = dati validi, qualsiasi altra cosa = dati corrotti/vuoti)
// Indirizzo 1: stato FSM (int, 4 byte su AVR)
// Indirizzo 5: pressione P0 (float, 4 byte)
constexpr int ADDR_MAGIC    = 0;
constexpr int ADDR_STATE    = 1;
constexpr int ADDR_PRESSURE = 5;
constexpr uint8_t MAGIC_BYTE = 0xAB;

// --- FUNZIONI INTERNE DI VALIDAZIONE ---
static bool is_valid_saved_state(FSM state) {
    int raw = static_cast<int>(state);
    return raw >= static_cast<int>(STATE_IDLE) &&
           raw <= static_cast<int>(STATE_LANDED);
}

static bool is_valid_saved_pressure(float p0) {
    return !isnan(p0) && !isinf(p0) && p0 >= 300.0F && p0 <= 1100.0F;
}

// --- IMPLEMENTAZIONI PUBBLICHE ---

SavedMission load_saved_mission() {
    SavedMission result;
    result.valid = false;
    result.state = STATE_IDLE;
    result.p0    = 1013.25F;

    // Controlla la firma: se non c'è, la EEPROM non è mai stata scritta
    uint8_t magic = EEPROM.read(ADDR_MAGIC);
    if (magic != MAGIC_BYTE) {
        return result; // Dati non validi, ritorna i default
    }

    // Leggi lo stato FSM
    int raw_state;
    EEPROM.get(ADDR_STATE, raw_state);
    FSM saved_state = static_cast<FSM>(raw_state);

    // Leggi la pressione P0
    float saved_p0;
    EEPROM.get(ADDR_PRESSURE, saved_p0);

    // Valida entrambi prima di accettarli
    if (!is_valid_saved_state(saved_state) || !is_valid_saved_pressure(saved_p0)) {
        return result; // Dati corrotti
    }

    result.valid = true;
    result.state = saved_state;
    result.p0    = saved_p0;
    return result;
}

void save_mission_state(FSM state) {
    EEPROM.write(ADDR_MAGIC, MAGIC_BYTE); // Scrive/conferma la firma
    EEPROM.put(ADDR_STATE, static_cast<int>(state));
}

void save_ground_pressure(float p0) {
    EEPROM.write(ADDR_MAGIC, MAGIC_BYTE); // Scrive/conferma la firma
    EEPROM.put(ADDR_PRESSURE, p0);
}

void clear_saved_mission() {
    EEPROM.write(ADDR_MAGIC, 0xFF); // Invalida la firma: al prossimo avvio riparte da zero
}