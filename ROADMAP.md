# Sestriere — Roadmap

Analisi dello stato attuale e piano di sviluppo futuro.
Ultimo aggiornamento: marzo 2026 (v1.8.0-beta)

---

## Feature completate

### 1. Contact Groups + Channel Muting
Contatti organizzati in gruppi nominati via SQLite (`contact_groups` + `contact_group_members`).
Flag mute persistente per contatti e canali. Right-click context menu.

### 2. GIF Animate via GIPHY
Invio/ricezione GIF animate tramite protocollo `g:{gifId}`, compatibile meshcore-open.
GIF Picker con griglia animata, cache locale, zero byte extra su LoRa.

### 3. Image Sharing via LoRa
Compressione color WebP a 192px, quality 50. Trasferimento chunked con auto-fetch.
Visualizzazione inline 250x300px. Retrocompatibile con vecchio formato JPEG.

### 4. SAR Markers
Parsing e visualizzazione marker SAR compatibili meshcore-sar, in chat e mappa.

### 5. Emoji Rendering
Rendering emoji Unicode tramite sprite PNG con alpha compositing.

### 6. OSM Map Tiles
Zoom Z2-Z18, cache disco 50 MB con eviction LRU, overlay stats, coastline fallback.

### 7. UI Settings Persistence
Filtri Chat/Repeater/Room salvati in `ui.settings`.

### 8. Voice Messages
Push-to-talk Codec2, envelope VE2, compatibile meshcore-sar.

### 9. Repeater Monitor → App standalone
Estratto da Sestriere come app indipendente in `repeater_monitor/`.

### 10. Fake Radio → App standalone
Spostato in `fake_radio/` con Makefile e icona HVIF.

---

## Bug fix / Robustezza (alta priorità)

### B1. Errori silenziosi su invio voice/immagini — COMPLETATO
- **Dove**: MainWindow.cpp — chiamate a `fProtocol->SendRawData()` e `SendDM()`
- **Problema**: il return status non viene controllato. L'utente vede "inviato" ma il messaggio potrebbe non partire
- **Fix**: controllato `status_t` di ritorno su SendDM (3 call site), SendChannelMsg (1), SendRawData (6 image + voice), SendRemoveContact, SendResetPath, SendSetChannel (2), SendRemoveChannel, SendAddUpdateContact. Image/voice transfer abortiti con stato FAILED e cleanup timer.
- **Stato**: completato v1.8.0-beta

### B2. Timeout connessione seriale — COMPLETATO
- **Dove**: SerialHandler::_ReadLoop()
- **Problema**: se il device si spegne, il read loop resta bloccato per sempre
- **Fix**: `select()` con timeout 1s prima di ogni `read()`. Se `select()` ritorna errore (fd invalido), notifica disconnessione e termina. Sul timeout, verifica fd con `ioctl(TIOCMGET)`. Contatore zero-read consecutivi: dopo `kMaxZeroReads` (30, ~3s) zero-read, dichiara disconnessione. Costante `kMaxZeroReads` in Constants.h.
- **Stato**: completato v1.8.0-beta

### B3. PUSH_CONTROL_DATA (0x8E) non gestito — COMPLETATO
- **Dove**: MainWindow::_ParseFrame() — manca il case per 0x8E
- **Problema**: unico push V3 ignorato. Messaggi di controllo dal device droppati silenziosamente
- **Fix**: aggiunto `case PUSH_CONTROL_DATA` in `_ParseFrame()` con `_HandlePushControlData()`. Parser: SNR (data[1]/4.0), RSSI (data[2]), path_len (data[3]), path, payload. Log nel debug log con categoria "CTRL" e hex dump (fino a 32 byte). Forward a MissionControl activity feed. Protezione frame corti. Copertura PUSH_* ora 15/15.
- **Stato**: completato v1.8.0-beta

### B4. Strip rimuove le risorse ELF
- **Dove**: processo di build release
- **Problema**: `strip` rimuove sezioni `.rsrc`, icone TopBar mancanti nei pacchetti distribuiti
- **Fix**: ri-applicare `xres -o binary binary.rsrc` dopo `strip`
- **Stato**: risolto in 1.8.0-beta

---

## Feature mancanti dal protocollo (media priorità)

### P1. Share Contact (CMD_SHARE_CONTACT) — COMPLETATO
- **Dove**: `SendShareContact()` esiste ma nessuna UI lo chiama
- **Fix**: aggiunta voce "Share Contact" nel context menu sidebar (right-click). Handler `MSG_CONTACT_SHARE` chiama `fProtocol->SendShareContact()` con check return value. Log successo/errore. Alert su fallimento. Solo per contatti non-canale.
- **Stato**: completato v1.8.0-beta

### P2. Custom Variables UI (GET/SET_CUSTOM_VARS)
- **Dove**: protocollo implementato, risposta loggata, nessuna interfaccia
- **Cosa fare**: tab "Custom Variables" in SettingsWindow con lista chiave/valore
- **Sforzo**: medio

### P3. Tuning Parameters UI (GET/SET_TUNING_PARAMS) — COMPLETATO
- **Dove**: comandi definiti, metodi in ProtocolHandler, mai esposti
- **Fix**: aggiunta sezione "Tuning" nel tab Device di SettingsWindow con campi RX Delay Base e Airtime Factor + pulsante "Apply Tuning". Message routing `MSG_SET_TUNING_PARAMS` → MainWindow → `ProtocolHandler::SendSetTuningParams()`. Check return value con log errore.
- **Stato**: completato v1.8.0-beta

### P4. Device PIN (SET_DEVICE_PIN)
- **Dove**: comando implementato, nessuna UI
- **Cosa fare**: campo PIN in SettingsWindow → tab Device
- **Sforzo**: basso

---

## Feature nuove (media priorità)

### F1. Compressione SMAZ per messaggi
- **Cosa**: compressione dizionario per testo chat (30-50% risparmio)
- **Formato**: prefisso `s:` per compatibilità meshcore-open
- **Perché**: critico su LoRa EU 868MHz con duty cycle 1%
- **Dipendenze**: libreria SMAZ (~150 righe C++, MIT license)
- **Difficoltà**: media
- **File**: nuovo Smaz.h/cpp, ProtocolHandler.cpp, MainWindow.cpp

### F2. Retry messaggi con backoff esponenziale
- **Cosa**: auto-retry quando `PUSH_SEND_CONFIRMED` non arriva entro timeout
- **Logica**: 3 tentativi con backoff (5s → 15s → 30s), poi fallimento
- **UI**: indicatore "tentativo 2/3" nel bubble del messaggio
- **Deduplicazione**: hash troncato per evitare duplicati lato ricevente
- **Difficoltà**: media-alta
- **File**: MainWindow.cpp (timer management), Types.h (delivery status)

### F3. Coda messaggi offline
- **Cosa**: accodare messaggi quando disconnessi, inviarli alla riconnessione
- **Dove**: DatabaseManager — tabella `outbox` con stato pending/sent/failed
- **UI**: badge "in coda" sui messaggi non ancora inviati
- **Difficoltà**: media

### F4. Download bulk tile mappa
- **Cosa**: pre-cache aree geografiche per uso offline
- **UI**: rettangolo di selezione sulla mappa + pulsante "Scarica area"
- **Limite**: rispettare cap 50 MB cache (o renderlo configurabile)
- **Difficoltà**: media
- **File**: MapView.cpp/h, TileCache.cpp/h

### F5. Line-of-Sight Analysis
- **Cosa**: profilo elevazione terreno tra due punti per verifica linea di vista
- **API**: Open-Meteo Elevation per campioni terreno
- **Calcoli**: curvatura terrestre (k-factor 4/3), zona di Fresnel
- **UI**: grafico profilo con terreno, LoS, zona Fresnel (verde=sgombro, rosso=ostruito)
- **Difficoltà**: alta
- **File**: nuovo LoSWindow.cpp/h, ElevationService.cpp/h

---

## UX / Persistenza (bassa priorità)

### U1. Persistere zoom/pan della mappa
- **Dove**: MapView.cpp — zoom level e centro in RAM
- **Fix**: salvare in `ui.settings` su zoom change, ricaricare all'apertura

### U2. Persistere larghezza sidebar e info panel
- **Dove**: MainWindow.cpp — BSplitView pesi
- **Fix**: salvare proporzioni in `ui.settings`, applicare dopo _BuildUI

### U3. VACUUM periodico del database
- **Dove**: DatabaseManager — non chiama mai VACUUM
- **Fix**: PRAGMA auto_vacuum = FULL alla creazione, oppure VACUUM al boot se > 10MB

### U4. Validazione tile cache corrotte
- **Dove**: TileCache::_LoadFromDisk()
- **Fix**: verificare magic bytes PNG (89 50 4E 47), eliminare e ri-scaricare se invalidi

### U5. Admin multi-repeater simultaneo
- **Dove**: MainWindow — `fLoggedInKey` è globale
- **Fix**: HashMap<pubkey, AdminSession> per sessioni parallele

---

## Testing

### T1. Test voice/image codec
Nessun test per VoiceCodec (encode/decode Codec2) e ImageCodec (WebP compress/decompress).
Test con payload noti, verifica round-trip.

### T2. Test SerialHandler read/write
Nessun test dedicato per assemblaggio frame e invio.
Test con PTY pair simulato (come fake_radio).

### T3. Test health score MissionControl
Calcolo health score non verificato.
Test con valori noti di SNR, RSSI, battery, uptime.

---

## Copertura protocollo V3

| Categoria | Implementati | Totale | Note |
|-----------|-------------|--------|------|
| CMD_* (inbound) | 39 | 39 | Tutti con UI |
| RSP_* (outbound) | 17 | 17 | Tutti gestiti |
| PUSH_* (notifiche) | 15 | 15 | Tutti gestiti |

---

## Valori hardcoded da rendere configurabili

| Parametro | Valore | Dove |
|-----------|--------|------|
| Tile cache max | 50 MB | TileCache.h: kMaxDiskCacheBytes |
| Retention messaggi DB | 30 giorni | DatabaseManager.cpp |
| Durata max voice | 30 secondi | VoiceSession.h: kMaxVoiceRecordSeconds |
| Finestra SNR chart | 24 ore | SNRChartView.cpp |
| WebP quality | 50 | ImageCodec.h |
| Immagine max dim | 192 px | ImageCodec.h |
| Media display max | 250×300 px | ChatView |
| MQTT publish interval | ~10 secondi | MqttClient.cpp |
| Memory tile cache | 32 tiles | TileCache.h |
| Pruning SNR history | 30 giorni | DatabaseManager.cpp |

---

## Priorità suggerita

| Priorità | ID | Feature | Difficoltà |
|----------|----|---------|-----------|
| **Alta** | B1 | Fix errori silenziosi voice/image | Bassa |
| **Alta** | B2 | Timeout connessione seriale | Bassa |
| **Alta** | B3 | Handler PUSH_CONTROL_DATA | Bassa |
| **Media** | P1 | Share Contact UI | Bassa |
| **Media** | F1 | Compressione SMAZ | Media |
| **Media** | F2 | Retry messaggi | Media-Alta |
| **Media** | P2 | Custom Variables UI | Media |
| **Media** | F3 | Coda messaggi offline | Media |
| **Bassa** | U1 | Persistere zoom mappa | Bassa |
| **Bassa** | U2 | Persistere larghezza sidebar | Bassa |
| **Bassa** | F4 | Download bulk tile | Media |
| **Bassa** | F5 | Line-of-Sight Analysis | Alta |

---

## Riferimenti

- meshcore-open: https://github.com/zjs81/meshcore-open
- SMAZ algorithm: https://github.com/antirez/smaz
- Open-Meteo Elevation API: https://open-meteo.com/en/docs/elevation-api
- CayenneLPP format: https://docs.mydevices.com/docs/lorawan/cayenne-lpp
- OSM tile server: https://tile.openstreetmap.org/{z}/{x}/{y}.png
