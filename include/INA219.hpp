#pragma once
#include <Arduino.h>
#include <Wire.h>

constexpr uint16_t DEFAULT_CONFIG_32V_320MV_CONTINUOUS = 0x399F;

struct InaSample { 
    float voltage_V = 0.0f;
    float current_mA = 0.0f;
    float power_mW = 0.0f;
    float energy_mWh = 0.0f;
    float remaining_pct = 0.0f;
};

enum class Ina219Error : uint8_t { 
    Ok = 0,
    NotInitialized,
    DeviceNotFound,
    InvalidCalibration,
    I2cWriteError,
    I2cReadError
};

class INA219BatteryMonitor {
public:
    
    INA219BatteryMonitor(uint8_t address = 0x40) : inaAddress(address) {}

    bool init_INA219(); // Inizializza il sensore INA219, restituendo true se l'inizializzazione ha avuto successo
    bool read_INA219(); // Legge i dati dal sensore INA219, aggiornando lastSample e restituendo true se la lettura ha avuto successo
    bool is_INA219_ready() const;
    InaSample getLastSample() const;
    void setBatteryCapacity(float capacity_mWh);
    void resetEnergyCounter();
    bool setCalibration(float shuntOhm, float maxCurrentA);
    void setInitialSocPercent(float socPct); // Imposta lo stato di carica iniziale in percentuale, calcolando l'energia consumata corrispondente
    float getConsumedWh() const;
    Ina219Error lastError() const;
    
private:
    bool writeRegister(uint8_t reg, uint16_t value);
    bool readRegister(uint8_t reg, uint16_t& value);

    bool inaReady = false;
    InaSample lastSample{};
    unsigned long lastReadTime = 0;
    float consumedEnergy_mWh = 0.0f;
    float batteryCapacity_mWh = 0.0f;
    float shuntOhm = 0.1f;
    float maxCurrentA = 2.0f;
    float currentLsb_A = 0.0f;
    float powerLsb_W = 0.0f;
    uint16_t calibrationReg = 0;
    
    
    uint8_t inaAddress; 
    
    Ina219Error lastErrorState = Ina219Error::Ok;
};
