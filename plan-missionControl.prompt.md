Obiettivo: migliorare la manutenzione e la chiarezza del modulo MissionControl per la missione CanSat.

1. Ridurre i globali in `MissionControl.cpp` spostando lo stato interno in una struttura o classe dedicata.
2. Raggruppare le costanti di soglia e temporizzazione in un singolo blocco di configurazione.
3. Uniformare i nomi: scegliere italiano o inglese per funzioni e commenti.
4. Estrarre l'inizializzazione di `current_data` in una funzione separata come `resetTelemetry()`.
5. Rendere la macchina a stati più leggibile, riducendo le nidificazioni in `update_mission_state()`.
6. Mantenere le funzioni `detect_*` isolate ma rendere più esplicito il comportamento di reset dei timer.
7. Valutare l'uso di una funzione helper per ricavare lo stato di discesa/paracadute e separare la logica dalla scrittura del servo.
8. Verificare eventuali punti di miglioramento negli accessi alle variabili condivise tra `main.cpp` e `MissionControl.cpp`.
9. Documentare le transizioni critiche nella FSM con commenti brevi e coerenti.
10. Preparare il refactor in piccole modifiche: primo passaggio `MissionControl.hpp` + `MissionControl.cpp`, poi `main.cpp`.
