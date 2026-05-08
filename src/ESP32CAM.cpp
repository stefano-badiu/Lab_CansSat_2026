#include "ESP32CAM.hpp"
#include "FS.h"       // File System base

bool init_camera_hardware() {
    // 1. SPEGNIMENTO FORZATO FLASH LED: Evita consumi e interferenze con la SD
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);
camera_config_t config = {};

config.ledc_channel = LEDC_CHANNEL_0;
config.ledc_timer = LEDC_TIMER_0;

config.pin_d0 = Y2_GPIO_NUM;
config.pin_d1 = Y3_GPIO_NUM;
config.pin_d2 = Y4_GPIO_NUM;
config.pin_d3 = Y5_GPIO_NUM;
config.pin_d4 = Y6_GPIO_NUM;
config.pin_d5 = Y7_GPIO_NUM;
config.pin_d6 = Y8_GPIO_NUM;
config.pin_d7 = Y9_GPIO_NUM;

config.pin_xclk = XCLK_GPIO_NUM;
config.pin_pclk = PCLK_GPIO_NUM;
config.pin_vsync = VSYNC_GPIO_NUM;
config.pin_href = HREF_GPIO_NUM;

config.pin_sccb_sda = SIOD_GPIO_NUM;
config.pin_sccb_scl = SIOC_GPIO_NUM;

config.pin_pwdn = PWDN_GPIO_NUM;
config.pin_reset = RESET_GPIO_NUM;

config.xclk_freq_hz = 20000000;
config.pixel_format = PIXFORMAT_JPEG;
config.sccb_i2c_port = 0;


    //logica della Memoria (PSRAM Check)
if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
} else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
}


    //Comando di accensione e controllo errori
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("ERRORE: Inizializzazione Camera Fallita! Codice: 0x%x", err);
        return false;
    }
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("ERRORE: Montaggio SD Fallito!");
        return false;
    } //Il "true" finale libera i pin 12 e 13 per la Seriale
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%per test va commentato finchè non abbiamo la sd
    

    if (!SD_MMC.exists("/telemetria_completa.csv")) {
        File headerFile = SD_MMC.open("/telemetria_completa.csv", FILE_WRITE);
        if (headerFile) {
            headerFile.println("TEAM_ID,MISSION_TIME,STATE,ALTITUDE,TEMPERATURE,GPS_LAT,GPS_LON,GPS_SATS,TILT_X,TILT_Y,TILT_Z,ACC_X,ACC_Y,ACC_Z,PARACHUTE_OPEN,BAT_VOLT,BAT_mA,BAT_mW,BAT_mWh,BAT_PCT,PRESSURE");
            headerFile.close();
            Serial.println("-> Nuovo file di telemetria creato con intestazione.");
        }
    }


/*
    // --- SETTAGGI FOTOGRAFICI MANUALI ---
    sensor_t * s = esp_camera_sensor_get();
    
    // 1. Disattiva gli automatismi
    s->set_exposure_ctrl(s, 0); // 0 = disattiva Auto-Esposizione (AEC)
    s->set_gain_ctrl(s, 0);     // 0 = disattiva Auto-Gain (AGC / ISO)
    
    // 2. Imposta i valori manuali
    s->set_agc_gain(s, 0);      // Gain al minimo (equivalente a ISO 100)
    s->set_aec_value(s, 300);   // Tempo di scatto manuale (DA CALIBRARE ALLA LUCE DEL SOLE! Più basso = più veloce/scuro)*/
    return true; // Se arriviamo qui, la camera è pronta!
    
}



void parse_incoming_data(const char* raw_string){
    // Analizziamo il primo carattere del pacchetto arrivato dal Nano
    
    if (raw_string[0] == 'P') {
        // CASO 1: È l'ordine di SCATTARE UNA FOTO (Prefisso 'P')
        // Esempio: "P;12000;150.5;2"
    Photo_Data dati_estratti = {0, 0.0f, STATE_IDLE};
    int state_temp = 0; // Variabile temporanea per leggere lo stato (che è un enum)

    // sscanf legge la stringa cercando il formato esatto.
    // %lu = unsigned long (Tempo), %f = float (Altitudine), %d = int (Stato)
    if (sscanf(raw_string+2, "%lu;%f;%d", &dati_estratti.MISSION_TIME, &dati_estratti.ALTITUDE, &state_temp) == 3) {
        dati_estratti.STATE = (FSM)state_temp; // Cast forzato
        capture_and_save(dati_estratti);       // Scatta la foto!
    } else {
        Serial.println("ERRORE: Pacchetto radio dal Nano illeggibile o corrotto.");
    }
}
else if (raw_string[0] == '<') {
    // Copia la stringa saltando il '<' iniziale
    char clean[256];
    strncpy(clean, raw_string + 1, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0'; // sicurezza: terminatore garantito

    // Rimuovi il '>' finale se presente
    int len = strlen(clean);
    if (len > 0 && clean[len - 1] == '>') {
        clean[len - 1] = '\0';
    }

    save_full_telemetry(clean);
}
}
// ---> NUOVA FUNZIONE PER SALVARE SOLO IL TESTO <---
void save_full_telemetry(const char* csv_string) {
    Serial.println("Telemetria ricevuta dal Nano (Salvataggio SD disabilitato per test)");
    // Apre il file principale, se non esiste lo crea
File dataFile = SD_MMC.open("/telemetria_completa.csv", FILE_APPEND);
    if (dataFile) {
        dataFile.println(csv_string); // Stampa l'intera stringa così com'è
        dataFile.close();
    } else {
        Serial.println("ERRORE: Impossibile scrivere la telemetria su SD.");
    }//%%%%%%%%%%%%%%%%%%%%%%%%%%%%per test va commentato finchè non abbiamo la sd
}

void capture_and_save(const Photo_Data &data) {

   // Creiamo due "recinti" di memoria sicuri (Buffer)
    char nome_file[32]; 
    char dati_csv[128]; 

    // Formattiamo le stringhe direttamente nei buffer
    // %lu = unsigned long, %.2f = float con 2 decimali, %d = int
    snprintf(nome_file, sizeof(nome_file), "/%lu.jpg", data.MISSION_TIME);
    snprintf(dati_csv, sizeof(dati_csv), "%s,%lu,%.2f,%d", nome_file, data.MISSION_TIME, data.ALTITUDE, (int)data.STATE);

    camera_fb_t * fb = esp_camera_fb_get(); 

    if (!fb) {
        Serial.println("Errore: La fotocamera non ha consegnato la foto!");
        return; 
    }
    Serial.println("CLICK! Comando foto ricevuto e frame catturato (Salvataggio SD disabilitato)");
    //apertura SD (Passiamo direttamente il buffer nome_file)
    File Pfile = SD_MMC.open(nome_file, FILE_WRITE);
    if (Pfile){
        Pfile.write(fb->buf, fb->len);
        Pfile.close();
    }
    else {
        Serial.println("Impossibile creare il file JPG");
    }
    // Questo è il mini-log specifico solo per catalogare le foto
    File CSV = SD_MMC.open("/log_foto.csv", FILE_APPEND);
    if (CSV){
        CSV.println(dati_csv); 
        CSV.close();
    } //%%%%%%%%%%%%%%%%%%%%%%%%%%%%per test va commentato finchè non abbiamo la sd
    esp_camera_fb_return(fb);

}