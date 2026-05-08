#include "BMP280.hpp"
#include <Wire.h> //Serve per attivare la comunicazione sui pin A4 (SDA) e A5 (SCL) dell'Arduino Nano, che sono dedicati al bus I2C. 
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h> //Contiene i comandi specifici per il chip BMP280
#include <MissionStorage.hpp> // Per salvare la pressione di riferimento P_0 e lo stato della missione in EEPROM
#include <Arduino.h>
#include <math.h>


extern Telemetry current_data;

Adafruit_BMP280 bmp;  // indica che ho creato un oggetto virtuale che rappresenta il chip fisico (l'ho capito così)

// Variabile globale del file per la pressione di riferimento
float P_0 = 1013.25;
float h_filt = 0.0F;
float v_vert = 0.0F;
float h_max = 0.0F;
namespace {
constexpr float ALPHA_H = 0.20F;
constexpr float ALPHA_V = 0.30F;
constexpr unsigned long MIN_DT = 20;

bool estimator_ready = false;
unsigned long last_altitude_time_ms = 0;
}


bool init_BMP280(){
    if (! bmp.begin(0x76)) {
        //Serial.println("ERRORE: BMP280 non trovato!"); // Messaggio per il team 
        return(false); }
    else  {
        // Configurazioni standard del sensore 
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     
                        Adafruit_BMP280::SAMPLING_X2,   //temperatura: campionamento standard  
                        Adafruit_BMP280::SAMPLING_X16, //il sensore fa 16 micro-letture interne e ne fa la media prima di dare il risultato  
                        Adafruit_BMP280::FILTER_X16,  //filtro matematico che "ammorbidisce" i cambiamenti bruschi.    
                        Adafruit_BMP280::STANDBY_MS_1); //tempo di riposo tra una misura automatica e l'altra
        return(true);  // questa è l'inizializzazione, avrò come risultato V/F.
}
}

void reset_BMP280_flight_estimator() {
    h_filt = 0.0F;
    v_vert = 0.0F;
    h_max = 0.0F;
    estimator_ready = false;
    last_altitude_time_ms = 0;
}

bool calibration_BMP280() {
    constexpr int NUM_LETTURE = 15;
    constexpr int MIN_LETTURE_VALIDE = 8;

    constexpr float P_MIN_HPA = 800.0F;
    constexpr float P_MAX_HPA = 1100.0F;

    float somma_P0 = 0.0F;
    int letture_valide = 0;

    for (int i = 0; i < NUM_LETTURE; i++) {
        float read_pressure = bmp.readPressure() / 100.0F;

        if (!isnan(read_pressure) &&
            !isinf(read_pressure) &&
            read_pressure > P_MIN_HPA &&
            read_pressure < P_MAX_HPA) {
            somma_P0 += read_pressure;
            letture_valide++;
        }

        delay(50);
    }

    if (letture_valide >= MIN_LETTURE_VALIDE) {
        P_0 = somma_P0 / static_cast<float>(letture_valide);
        save_ground_pressure(P_0);
        reset_BMP280_flight_estimator();
        return true;
    }

    // Fallita: non tocchiamo P_0, non tocchiamo EEPROM.
    // Rimane in uso l'ultimo P_0 valido già presente in RAM.
    return false;
}


void read_BMP280(){
    unsigned long now = millis();
    float current_pressure = bmp.readPressure()/100.0F; // nella formula mi serve la Pressione trovata dal sensore, essa viene misurata in Pa, a me serve in hPa, perciò /100
    float base = current_pressure/P_0; //calcoliamo il rapporto rispetto alla pressione di terra (P/P0)
    float delta = 1.0F - base;   // Sostituiamo pow() con un'approssimazione polinomiale quadratica, messun logaritmo, calcolo istantaneo per la CPU a 8-bit
    float altitude = (8435.8F * delta) + (3415.3F * delta * delta);

    if (!estimator_ready) { // alla prima lettura, inizializziamo il filtro con l'altitudine attuale
        h_filt = altitude;
        v_vert = 0.0F;
        h_max = altitude;
        estimator_ready = true;
        last_altitude_time_ms = now;
    } else {
        unsigned long dt_ms = now - last_altitude_time_ms;

        if (dt_ms >= MIN_DT) { // Evitiamo divisioni per zero o tempi troppo brevi che causerebbero stime instabili
            float previous_altitude_filtered = h_filt;

            h_filt += ALPHA_H * (altitude - h_filt); //filtro di Kalman semplificato: la nuova stima è la vecchia più una correzione proporzionale all'errore (differenza tra misura e stima)
            float vertical_speed_raw = (h_filt - previous_altitude_filtered) / (dt_ms / 1000.0F);
            v_vert += ALPHA_V * (vertical_speed_raw - v_vert);
            last_altitude_time_ms = now;

            if (h_filt > h_max) { // aggiorno la quota massima raggiunta
                h_max = h_filt;
            }
        }
    }
 
    // aggiornamento della telemetria globale
    current_data.TEMPERATURE = bmp.readTemperature();
    current_data.ALTITUDE= h_filt; //assegno il dato raccolto nella raccolta
    current_data.PRESSURE=current_pressure;
    current_data.VERTICAL_SPEED= v_vert; // se vogliamo inviare anche la velocità verticale, dobbiamo aggiungerla alla struct Telemetry e al pacchetto di telemetria che inviamo a terra
}
