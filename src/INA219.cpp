#include "INA219.hpp"

namespace {

constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_BUS_VOLTAGE = 0x02;
constexpr uint8_t REG_POWER = 0x03;
constexpr uint8_t REG_CURRENT = 0x04;
constexpr uint8_t REG_CALIBRATION = 0x05;

} 

bool INA219BatteryMonitor::writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(inaAddress);
    Wire.write(reg);
    Wire.write(static_cast<uint8_t>((value >> 8) & 0xFF)); // MSB
    Wire.write(static_cast<uint8_t>(value & 0xFF));        // LSB

    if (Wire.endTransmission() != 0) {
        lastErrorState = Ina219Error::I2cWriteError;
        return false;
    }

    lastErrorState = Ina219Error::Ok;
    return true;
}

bool INA219BatteryMonitor::readRegister(uint8_t reg, uint16_t& value) {
    Wire.beginTransmission(inaAddress);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0) { // repeated start
        lastErrorState = Ina219Error::I2cWriteError;
        return false;
    }

    if (Wire.requestFrom(static_cast<int>(inaAddress), 2) != 2) { // Dovrebbe restituire 2 byte
        lastErrorState = Ina219Error::I2cReadError;
        return false;
    }

    const uint8_t msb = static_cast<uint8_t>(Wire.read()); // Legge il byte più significativo
    const uint8_t lsb = static_cast<uint8_t>(Wire.read()); // Legge il byte meno significativo
    value = static_cast<uint16_t>((static_cast<uint16_t>(msb) << 8) | lsb); // Combina MSB e LSB

    lastErrorState = Ina219Error::Ok;
    return true;
}

bool INA219BatteryMonitor::setCalibration(float shuntOhm, float maxCurrentA) {
    if (shuntOhm <= 0.0f || maxCurrentA <= 0.0f) { // Valori non validi per la calibrazione
        lastErrorState = Ina219Error::InvalidCalibration;
        return false;
    } 

    const float newCurrentLsb_A = maxCurrentA / 32768.0f; // La corrente massima attesa divisa per 2^15 (perché il registro corrente è un intero con segno a 16 bit) determina il valore di LSB per la corrente
    if (newCurrentLsb_A <= 0.0f) { // Controllo di sicurezza per evitare divisioni per zero o valori non validi
        lastErrorState = Ina219Error::InvalidCalibration;
        return false;
    }

    const float calF = 0.04096f / (newCurrentLsb_A * shuntOhm); // Formula di calibrazione per ottenere il valore da scrivere nel registro di calibrazione
    if (calF < 1.0f || calF > 65535.0f) { // Il valore di calibrazione deve essere un intero a 16 bit
        lastErrorState = Ina219Error::InvalidCalibration;
        return false;
    }

    this->shuntOhm = shuntOhm; // Aggiorna la resistenza shunt
    this->maxCurrentA = maxCurrentA; // Aggiorna la corrente massima attesa
    currentLsb_A = newCurrentLsb_A;
    powerLsb_W = currentLsb_A * 20.0f;
    calibrationReg = static_cast<uint16_t>(calF);

    if (inaReady && !writeRegister(REG_CALIBRATION, calibrationReg)) {
        return false;
    }

    lastErrorState = Ina219Error::Ok;
    return true;
}

bool INA219BatteryMonitor::init_INA219() {
    
    uint16_t configValue; // Variabile temporanea per testare la comunicazione
    if (!readRegister(REG_CONFIG, configValue)) { // Verifica se il dispositivo risponde
        lastErrorState = Ina219Error::DeviceNotFound;
        inaReady = false;
        return false;
    }

    if (!setCalibration(shuntOhm, maxCurrentA)) { // Imposta la calibrazione con i valori correnti
        inaReady = false;
        return false;
    }

    if (!writeRegister(REG_CALIBRATION, calibrationReg)) { // Scrive il valore di calibrazione
        inaReady = false;
        return false;
    }

    if (!writeRegister(REG_CONFIG, DEFAULT_CONFIG_32V_320MV_CONTINUOUS)) { // Configurazione operativa
        inaReady = false;
        return false;
    }

    inaReady = true;
    lastErrorState = Ina219Error::Ok;
    lastReadTime = millis(); // Inizializza il timer di lettura
    return true;
}

bool INA219BatteryMonitor::read_INA219() {

    if (!inaReady) {
        lastErrorState = Ina219Error::NotInitialized;
        return false;
    }
    const unsigned long now = millis();
    if (now - lastReadTime < 100) { // Limita la frequenza di lettura a 10Hz
        return true; // Non è un errore, semplicemente non è ancora il momento di leggere
    }
    
    uint16_t rawBusVoltage = 0;
    uint16_t rawCurrent = 0;
    uint16_t rawPower = 0;

    if (!readRegister(REG_BUS_VOLTAGE, rawBusVoltage)) {
        return false;
    }
    if (!readRegister(REG_CURRENT, rawCurrent)) {
        return false;
    }
    if (!readRegister(REG_POWER, rawPower)) {
        return false;
    }

    const float dtHours = (lastReadTime > 0) ? static_cast<float>(now - lastReadTime) / 3600000.0f : 0.0f;

    const float busV = static_cast<float>((rawBusVoltage >> 3) * 4) / 1000.0f; // Il registro bus voltage è in unità di 4mV, quindi si scarta i 3 bit meno significativi e si moltiplica per 4mV, poi si converte in Volt
    const int16_t currentRawSigned = static_cast<int16_t>(rawCurrent);
    const float currentA = static_cast<float>(currentRawSigned) * currentLsb_A;
    const float powerW = static_cast<float>(rawPower) * powerLsb_W;

    lastSample.voltage_V = busV;
    lastSample.current_mA = currentA * 1000.0f;
    lastSample.power_mW = powerW * 1000.0f;

    if (dtHours > 0.0f && lastSample.power_mW > 0.0f) { // Aggiorna l'energia consumata solo se è passato del tempo e se c'è consumo di potenza
        consumedEnergy_mWh += lastSample.power_mW * dtHours; // Energia = Potenza * Tempo, convertendo da mW e ore a mWh
    }

    lastSample.energy_mWh = consumedEnergy_mWh;

    if (batteryCapacity_mWh > 0.0f) {
        float remaining = 100.0f * (1.0f - (consumedEnergy_mWh / batteryCapacity_mWh));
        lastSample.remaining_pct = constrain(remaining, 0.0f, 100.0f);
    } else {
        lastSample.remaining_pct = 0.0f;
    }

    lastReadTime = now;
    lastErrorState = Ina219Error::Ok;
    return true;
}

void INA219BatteryMonitor::setBatteryCapacity (float capacity_mWh) {
    if (capacity_mWh < 0.0f) { // Valore non valido per la capacità della batteria
        batteryCapacity_mWh = 0.0f;
    } else {
        batteryCapacity_mWh = capacity_mWh; // Aggiorna la capacità della batteria
    }
    if (batteryCapacity_mWh > 0.0f) { // Calcola la percentuale di carica residua basata sull'energia consumata e sulla capacità totale
        float remaining = 100.0f * (1.0f - (consumedEnergy_mWh / batteryCapacity_mWh));
        lastSample.remaining_pct = constrain(remaining, 0.0f, 100.0f);
    } else {
        lastSample.remaining_pct = 0.0f;
    }
}

void INA219BatteryMonitor::setInitialSocPercent (float socPct) { 
    if (socPct < 0.0f) socPct = 0.0f; // Valore non valido per la percentuale di carica residua
    if (socPct > 100.0f) socPct = 100.0f;

    if (batteryCapacity_mWh > 0.0f) {
        consumedEnergy_mWh = batteryCapacity_mWh * (1.0f - socPct / 100.0f);
        lastSample.remaining_pct = socPct;
    } else {
        consumedEnergy_mWh = 0.0f;
        lastSample.remaining_pct = 0.0f;
    }
    lastSample.energy_mWh = consumedEnergy_mWh;
}

void INA219BatteryMonitor::resetEnergyCounter() {
    consumedEnergy_mWh = 0.0f; // Resetta l'energia consumata a zero
    lastSample.energy_mWh = 0.0f;
    if (batteryCapacity_mWh > 0.0f) { // Se la capacità della batteria è definita, resetta anche la percentuale di carica residua a 100%
        lastSample.remaining_pct = 100.0f;
    } else {
        lastSample.remaining_pct = 0.0f;
    }
    lastReadTime = millis(); // Resetta il timer di lettura per evitare grandi delta al primo aggiornamento
}

float INA219BatteryMonitor::getConsumedWh() const { // Restituisce l'energia consumata in Watt-ora, convertendo da milliWatt-ora
    return consumedEnergy_mWh / 1000.0f; // Converti mWh in Wh
}

bool INA219BatteryMonitor::is_INA219_ready() const {
    return inaReady;
}

InaSample INA219BatteryMonitor::getLastSample() const { // Restituisce l'ultimo campione letto dal sensore
    return lastSample;
}

Ina219Error INA219BatteryMonitor::lastError() const { // Restituisce l'ultimo stato di errore verificatosi durante le operazioni con il sensore
    return lastErrorState;
}
