#include <Arduino.h>
#include "Sputnik_Identity.hpp"
#include "BMP280.hpp"
#include "MPU6050.hpp"
#include "GPS_PA6H.hpp"
#include "Xbee_S2C.hpp"
#include "MicroSD.hpp"
#include <Wire.h>



Telemetry current_data;

FSM currentState = STATE_IDLE; //inizializzazione stato missione


void setup() {
    Serial.begin(9600); // Apre la comunicazione seriale per vedere i dati a schermo
    Wire.begin();
    while (!Serial); // Attende che la porta seriale sia pronta (solo per test)

    Serial.println("--- SPUTNIK_1: TEST SISTEMI ---");

    if (!init_BMP280()) {
        Serial.println("AVVISO: BMP280 non risponde.");
        
    } else {
        Serial.println("BME280 inizializzato con successo.");
    }
}

void loop() {
    read_BMP280(); 

    // Stampiamo i risultati per vederli
    Serial.print("Altitudine: ");
    Serial.print(current_data.ALTITUDE);
    Serial.print(" m | Temp: ");
    Serial.print(current_data.TEMPERATURE);
    Serial.println(" *C");

    delay(1000);  
}

