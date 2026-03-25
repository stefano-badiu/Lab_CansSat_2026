#include "MPU6050.hpp"   // Header del modulo MPU6050
#include <Wire.h>        // Libreria per comunicazione I2C
#include <math.h>        // Serve per sqrtf() e acosf()
#include <Arduino.h>     // Serve per Serial, RAD_TO_DEG e tipi Arduino

static constexpr uint8_t kMpuAddress = 0x68;  // Indirizzo I2C standard del MPU6050
static bool mpuReady = false;                 // Tiene traccia se il sensore è pronto oppure no


static int16_t readAxisRegister(uint8_t registerAddress) {
    Wire.beginTransmission(kMpuAddress);  // Inizia comunicazione col sensore
    Wire.write(registerAddress);          // Seleziona il registro da leggere

    if (Wire.endTransmission(false) != 0) {  // false = non rilascia il bus, utile per lettura immediata
        return 0;                            // Se c'è errore, restituisce 0
    }

    if (Wire.requestFrom(static_cast<int>(kMpuAddress), 2) != 2) {  // Chiede 2 byte dal sensore
        return 0;                                                    // Se non arrivano 2 byte, errore
    }

    const int16_t highByte = Wire.read();  // Legge il byte alto
    const int16_t lowByte = Wire.read();   // Legge il byte basso

    return static_cast<int16_t>((highByte << 8) | lowByte);  // Ricompone il valore a 16 bit
}

static float clampUnit(float value) {
    if (value > 1.0F) return 1.0F;    // Limita il valore massimo a +1
    if (value < -1.0F) return -1.0F;  // Limita il valore minimo a -1
    return value;                     // Se è già nel range [-1, 1], lo restituisce invariato
}

bool init_MPU6050() {
    Wire.beginTransmission(kMpuAddress);  // Inizia comunicazione con MPU6050
    Wire.write(0x6B);                     // Registro PWR_MGMT_1
    Wire.write(0x00);                     // Scrive 0 per "svegliare" il sensore

    if (Wire.endTransmission() != 0) {    // Se la trasmissione fallisce
        Serial.println(F("ERRORE: MPU6050 non trovato."));
        mpuReady = false;
        return false;
    }

    mpuReady = true;                      // Il sensore è stato trovato e inizializzato
    return true;
}

void read_MPU6050() {
    if (!mpuReady) return;  // Se il sensore non è pronto, esce subito

    const float ax = static_cast<float>(readAxisRegister(0x3B)) / 16384.0F;  // Accelerazione asse X in g
    const float ay = static_cast<float>(readAxisRegister(0x3D)) / 16384.0F;  // Accelerazione asse Y in g
    const float az = static_cast<float>(readAxisRegister(0x3F)) / 16384.0F;  // Accelerazione asse Z in g

    const float magnitude = sqrtf((ax * ax) + (ay * ay) + (az * az));  // Modulo del vettore accelerazione

    if (magnitude <= 0.0F) return;  // Evita divisione per zero o valori non validi

    current_data.TILT_X = acosf(clampUnit(ax / magnitude)) * (180.0f / M_PI);  // Angolo rispetto all'asse X in gradi
    current_data.TILT_Y = acosf(clampUnit(ay / magnitude)) * (180.0f / M_PI);  // Angolo rispetto all'asse Y in gradi
    current_data.TILT_Z = acosf(clampUnit(az / magnitude)) * (180.0f / M_PI);  // Angolo rispetto all'asse Z in gradi
}

bool is_MPU6050_ready() {
    return mpuReady;  // Restituisce lo stato di inizializzazione del sensore
}
