#include "GPS_PA6H.hpp"
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);
TinyGPSPlus GPS;

void focus_GPS() {
    gpsSerial.listen();
}

bool init_GPS() {
    gpsSerial.begin(9600);
    gpsSerial.println("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28");
    gpsSerial.println("$PMTK220,1000*1F");

    // Aspetta fino a 2 secondi che il modulo risponda con qualcosa
    unsigned long start = millis();
    while (millis() - start < 2000) {
        if (gpsSerial.available() > 0) {
            return true; // Il modulo sta trasmettendo
        }
    }
    return false; // Nessun dato: modulo assente o scollegato
}

void update_GPS_data(){
   while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    if (GPS.encode(c)){
        if (GPS.location.isValid()){
            current_data.GPS_LATITUDE = GPS.location.lat();
            current_data.GPS_LONGITUDE = GPS.location.lng();
        }
        if(GPS.satellites.isValid()){
            current_data.GPS_SATS= (int)GPS.satellites.value();
        }
    }
 }
}

