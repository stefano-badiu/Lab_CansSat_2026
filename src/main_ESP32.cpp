#include <Arduino.h>
#include "ESP32CAM.hpp" // La tua libreria personalizzata

// --- INTERRUTTORE DI MISSIONE ---
#define MOD_TEST // %%%%%%%%%%%%Commenta questa riga per il lancio reale%%%%%%%%%%%%%
// 1. Definiamo la nuova seriale hardware
// UART1 dell'ESP32: RX su pin 13, TX su pin 12 (opzionale)
HardwareSerial SerialNano(1); 
#define RX_NANO 13
#define TX_NANO 12

void setup() {
   // Porta di DEBUG (USB) - Alziamo la velocità per i log pesanti
    Serial.begin(115200);
    SerialNano.begin(19200, SERIAL_8N1, RX_NANO, TX_NANO);
    
// 2. Logica di avvio per il LANCIO (Modalità Silenziosa)
 delay(2000); // Pausa di sicurezza per far stabilizzare le tensioni della batteria
init_camera_hardware(); 
}

void loop() {
    // Buffer statico per la ricezione (addio classe String!)
    static char buffer[256];
    static int index = 0;
    
    while (SerialNano.available() > 0) {
        char c = SerialNano.read();

        // Se troviamo il carattere di fine riga (\n) o il buffer è pieno
        if (c == '\n' || index >= 254) {
            buffer[index] = '\0'; // Chiudiamo la stringa
            
            #ifdef MOD_TEST
                Serial.print("Dato ricevuto dal Nano: ");
                Serial.println(buffer);
            #endif
            
            // In volo o in test, il parser viene chiamato SEMPRE e solo UNA volta
            parse_incoming_data(buffer); 
            
            index = 0; // Resettiamo il buffer per il prossimo pacchetto
        } 
        else if (c != '\r') { // Ignoriamo il ritorno a capo di Windows
            buffer[index++] = c;
        }
    }
}