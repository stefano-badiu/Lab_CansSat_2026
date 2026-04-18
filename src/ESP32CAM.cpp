#include "ESP32CAM.hpp"
#include "FS.h"       // File System base
#include "SD_MMC.h"
#include <WiFi.h>
#include "esp_http_server.h"

bool init_camera_hardware() {
    camera_config_t config;

    // Mappatura dei PIN (Traduzione dal .hpp alla libreria)
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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    //parametri fissi di sistema
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; // Usiamo sempre JPEG

    //logica della Memoria (PSRAM Check)
    if (psramFound()) {
        // Se la memoria extra funziona, impostiamo i parametri per il Wi-Fi fluido
        config.frame_size = FRAMESIZE_VGA; 
        config.jpeg_quality = 12; 
        config.fb_count = 2; // Doppio buffer per streaming fluido
        config.grab_mode = CAMERA_GRAB_LATEST; // Prende sempre l'ultima foto scattata
    } else {
        // Modalità emergenza: poca memoria disponibile
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    //Comando di accensione e controllo errori
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("ERRORE: Inizializzazione Camera Fallita! Codice: 0x%x", err);
        return false;
    }
    if (!SD_MMC.begin()) {
        Serial.println("ERRORE: Montaggio SD Fallito!");
        return false;
    }

    return true; // Se arriviamo qui, la camera è pronta!
}

void parse_incoming_data(String raw_string){
    raw_string.trim(); // <--- Pulisce gli "a capo" invisibili prima di iniziare
    Photo_Data dati_estratti;

    int separatore = raw_string.indexOf(";"); //troviamo in che posizione si trova il primo ';'
    String tempo =raw_string.substring(0,separatore); //ritagliamo il pezzo di stringa da zero fino al separatore
    dati_estratti.MISSION_TIME=tempo.toInt();
    raw_string=raw_string.substring(separatore+1); //ho messo il +1 perchè sennò avrei incluso anche il ;
    separatore=raw_string.indexOf(";");
    String altitude=raw_string.substring(0,separatore);
    dati_estratti.ALTITUDE=altitude.toFloat();
    raw_string = raw_string.substring(separatore + 1);
    dati_estratti.STATE=(FSM)raw_string.toInt(); // Cast forzato a FSM

    capture_and_save(dati_estratti);  //scatta
}

void capture_and_save(const Photo_Data &data) {

    String nome_file = "/"+ String(data.MISSION_TIME)+ ".jpg"; // Nome del file basato sul tempo di missione
    String dati_csv = nome_file + "," + String(data.MISSION_TIME) + "," + String(data.ALTITUDE) + "," + String(data.STATE);
    camera_fb_t * fb = esp_camera_fb_get(); 

    if (!fb) {
    Serial.println("Errore: La fotocamera non ha consegnato la foto!");
    return; // Interrompe la funzione se lo scatto fallisce
    }
    //apertura SD
    File Pfile = SD_MMC.open(nome_file.c_str(), FILE_WRITE);
    if (Pfile){
        Pfile.write(fb->buf, fb->len);
        Pfile.close();
    }
    else {
        Serial.println("Impossibile creare il file JPG");
    }
    File CSV = SD_MMC.open("/dati_csv.csv", FILE_APPEND);

    if (CSV){
        CSV.println(dati_csv);
        CSV.close();
    }
    esp_camera_fb_return(fb);

}



// Inserisci qui i dati del tuo Wi-Fi o dell'Hotspot del telefono
const char* ssid = "Galaxy 24 Ultra";
const char* password = "Orso2004";

httpd_handle_t stream_httpd = NULL;

// Questa è la funzione "operaia" che impacchetta le foto per il browser
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    char part_buf[64];

    // Diciamo al browser: "Preparati, sta arrivando un flusso video continuo"
    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
    if(res != ESP_OK){ return res; }

    // Ciclo infinito: scatta e invia
    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Errore: Scatto fallito durante lo streaming");
            res = ESP_FAIL;
        } 
        
        if(res == ESP_OK){
            // Impacchettiamo il singolo fotogramma JPEG
            size_t hlen = snprintf(part_buf, 64, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if(res == ESP_OK){
            // Inviamo l'immagine vera e propria
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, "\r\n", 2);
        }
        
        // Restituiamo SEMPRE il buffer alla memoria, altrimenti la scheda va in crash!
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        
        // Se il browser viene chiuso, usciamo dal ciclo
        if(res != ESP_OK){
            break; 
        }
    }
    return res;
}

// Questa è la funzione che richiamerai dal tuo main
void setup_wifi_stream() {
    Serial.print("Connessione al WiFi: ");
    Serial.println(ssid);
    
    // Disattiviamo il risparmio energetico del Wi-Fi per evitare lag nel video
    WiFi.setSleep(false); 
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connesso!");
    Serial.print("-> VAI SUL BROWSER A QUESTO INDIRIZZO: http://");
    Serial.println(WiFi.localIP());

    // Avviamo il Server Web sulla porta 80
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &index_uri);
    }
}