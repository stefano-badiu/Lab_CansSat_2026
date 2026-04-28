#pragma once
#include "Sputnik_Identity.hpp"
#include <esp_camera.h>
#include <WiFi.h>
#include "FS.h"      
#include "SD_MMC.h"

// Definizione dei PIN per modello AI-Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1 // Il reset è gestito dal tasto fisico, non via software
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Pin del Flash LED (quello bianco potente)
#define FLASH_LED_PIN      4

struct Camera_Config {
    int xclk_freq; // Frequenza dell'XCLK (tipicamente 20MHz o 10MHz)
    int frame_size;   // Dimensione del frame (es. FRAMESIZE_SVGA)
    int pixel_format; // Formato dei pixel (es. PIXFORMAT_JPEG)
    int fb_count; // Numero di frame buffer (1 o 2)
    int grab_mode; // Modalità di acquisizione (es. CAMERA_GRAB_LATEST)
    int jpeg_quality; // Qualità JPEG (0-63, dove 0 è la migliore qualità)
};


struct Photo_Data {
    unsigned long MISSION_TIME; // Tempo di missione al momento dello scatto
    float ALTITUDE; // Altitudine al momento dello scatto
    FSM STATE; // Stato della FSM al momento dello scatto    
};
void parse_incoming_data(const char* raw_string); // Funzione per il parsing dei dati in arrivo, che poi verranno salvati nella telemetria e usati per la funzione capture_and_save
 
bool init_camera_hardware();
void setup_wifi_stream();

/* vvvvvvvvvvvQuesta funzione scatta la foto e la salva.vvvvvvvvvvvv
   Usiamo 'const Photo_Data &data' per due motivi tecnici:
   1. La '&' (Riferimento): evita di copiare i dati in RAM, risparmiando memoria e tempo.
   2. Il 'const' (Costante): garantisce che la funzione legga solo i dati senza poterli modificare per errore.
*/
void capture_and_save(const Photo_Data &data);
