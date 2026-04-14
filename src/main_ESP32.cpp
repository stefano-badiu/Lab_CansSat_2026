#include <Arduino.h>
#include "ESP32CAM.hpp" // La tua libreria personalizzata

// --- INTERRUTTORE DI MISSIONE ---
#define MOD_TEST // %%%%%%%%%%%%Commenta questa riga per il lancio reale%%%%%%%%%%%%%

void setup() {
   
    Serial.begin(9600); 

    #ifdef MOD_TEST
        delay(1000); // Pausa per far stabilizzare il monitor seriale
        Serial.println("\n--- SPUTNIK-33cl: CAMERA SYSTEM CHECK (TEST) ---");
        
        if (init_camera_hardware()) {
            Serial.println("Hardware (Camera + SD): OK.");
            Serial.println("In attesa di ordini di scatto...");
        } else {
            Serial.println("ERRORE CRITICO: Inizializzazione Hardware fallita.");
            Serial.println("Controllare la MicroSD e il montaggio della lente.");
        }
    #else
        // 2. Logica di avvio per il LANCIO
        delay(2000); // Pausa di sicurezza per far stabilizzare le tensioni della batteria
        init_camera_hardware(); 
    #endif
}

void loop() {
   
    // Se c'è corrente sul cavo RX, il Nano sta mandando un pacchetto
    if (Serial.available() > 0) {
        
        // Leggiamo tutto il pacchetto finché non troviamo (\n)
        String messaggio_in_arrivo = Serial.readStringUntil('\n');

    #ifdef MOD_TEST
        Serial.print("Ordine ricevuto dal Nano: ");
        Serial.println(messaggio_in_arrivo);
    
        parse_incoming_data(messaggio_in_arrivo); 
        
        Serial.println("-> Foto catturata, SD aggiornata, Memoria RAM liberata.");
        Serial.println("---------------------------------------------------");
    #else
        // In Volo: .
        // Prende la stringa, scatta e si rimette in attesa.
        parse_incoming_data(messaggio_in_arrivo); 
    #endif

    }
}