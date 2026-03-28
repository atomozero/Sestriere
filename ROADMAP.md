# Sestriere — Roadmap

Analisi dello stato attuale e piano di sviluppo futuro.
Ultimo aggiornamento: marzo 2026 (v1.9.2)

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

### 11. Line-of-Sight Analysis
Profilo elevazione terreno tra due nodi, zona di Fresnel, curvatura terrestre (k-factor 4/3).
API Open-Meteo per dati elevazione. File: LoSAnalysis.h, ElevationService.cpp/h, LoSWindow.cpp/h.

### 12. Multi-Companion Support
DB partizionato per companion radio: colonna `companion_key` su tabelle `messages` e `snr_history`.
Al disconnect: clear totale di contatti, chat, telemetria, stats, network map, packet analyzer.
Riconnessione: messaggi ricaricati dal DB filtrati per identità companion.
Retrocompatibile: dati esistenti (companion_key vuota) accessibili da qualsiasi companion.

### 13. TelemetryWindow Redesign
Dashboard con card-based layout (CardWrapperView), flow layout, auto-sizing MetricCardView.

### 14. Mission Control Improvements
Ping All e Trace Route quick actions, health arc inline, card style unificato,
timeline sessione, activity feed, footer last-update con stale detection.

---

## Bug fix completati

### B1. Errori silenziosi su invio voice/immagini — v1.8.0-beta
Controllato `status_t` di ritorno su SendDM, SendChannelMsg, SendRawData, etc.
Image/voice transfer abortiti con stato FAILED e cleanup timer.

### B2. Timeout connessione seriale — v1.8.0-beta
`select()` con timeout 1s, verifica fd con `ioctl(TIOCMGET)`, contatore zero-read.

### B3. PUSH_CONTROL_DATA (0x8E) non gestito — v1.8.0-beta
Aggiunto handler con parser SNR/RSSI/path/payload. Copertura PUSH_* 15/15.

### B4. Strip rimuove le risorse ELF — v1.8.0-beta
Ri-applicare `xres -o binary binary.rsrc` dopo `strip`.

### B5. Null dereference, timer leak, sqlite3_close — v1.9.1
Guardie Window()==NULL, TopBarView no leak, delete timer nel distruttore, sqlite3_close_v2.

### B6. Serial Monitor forward frame binari — v1.9.1
Aggiunto `_ForwardFrameToSerialMonitor()` per frame TX/RX.

### B7. BBitmap InitCheck mancante in drag & drop — v1.9.2
InitCheck() + delete + early return per copy e thumbnail bitmap.

### B8. VLA stack overflow in SNRChartView — v1.9.2
Sostituito VLA con `new BPoint[]` + `delete[]`.

### B9. WindowLocker RAII e _ShowWindow null-safe — v1.9.2
Classe WindowLocker RAII applicata a 5 finestre. Null check su _ShowWindow.

### B10. Divide-by-zero in LoS Analysis — v1.9.2
Guardia `totalDist <= 0` prima dei loop Fresnel.

### B11. strncpy in MissionControlWindow — v1.9.2
Sostituito con `strlcpy()`.

### B12. Thread leak in GifPickerWindow — v1.9.2
Fix lifecycle fSearchThread.

### B13. getenv("HOME") senza null check in MapView — v1.9.2
Sostituito con `find_directory(B_USER_SETTINGS_DIRECTORY)`.

### P1. Share Contact UI — v1.8.0-beta
Context menu "Share Contact" nella sidebar.

### P3. Tuning Parameters UI — v1.8.0-beta
Sezione "Tuning" nel tab Device di SettingsWindow.

### P4. Device PIN UI — v1.8.0-beta
Campo "BLE PIN" nel tab Device di SettingsWindow.

---

## Bug aperti (segnalati da utenti)

### S1. Doppio `#` nei nomi canali hashtag — [#2](https://github.com/atomozero/Sestriere/issues/2) — COMPLETATO
- **Segnalato da**: serwin2 (scotty3g)
- **Dove**: `MainWindow.cpp` — `_FilterContacts()`, header display
- **Problema**: `label.SetToFormat("#%s", ch->name)` prepende `#` incondizionatamente. Se il device restituisce il nome già con `#` (es. `#test`), viene visualizzato `##test`.
- **Fix**: check `ch->name[0] == '#'` prima di prependerlo, in `_FilterContacts()` e nell'header display.
- **Stato**: completato (commit 65f37bf)

### S2. Public Channel duplicato e non funzionante — [#3](https://github.com/atomozero/Sestriere/issues/3) — COMPLETATO
- **Segnalato da**: serwin2 (scotty3g)
- **Problema**: "Public Channel" hardcoded duplicava il canale enumerato dal device
- **PSK well-known**: `8b3387e9c5cdea6ac9e5edbaa115cd72` → `kPublicChannelPSK` in Constants.h
- **Fix**: rimossi `fChannelItem` e `fChannelMessages`. Tutti i canali (incluso Public a index 0) gestiti uniformemente via `fChannels`. Aggiunta voce menu "Add Public Channel" con PSK well-known e check duplicato. Migrazione messaggi dal vecchio DB key `"channel"` al nuovo `"channel_0"`. Mute key unificata a `"ch_0"`.
- **Stato**: completato (commit bca43f1)

---

## Feature richieste da utenti

### S3. PSK custom/random per canali privati — [#4](https://github.com/atomozero/Sestriere/issues/4)
- **Segnalato da**: serwin2 (scotty3g)
- **Dove**: `AddChannelWindow.cpp/h`
- **Stato attuale**: solo campo "Name", PSK generata automaticamente con SHA-256 del nome. Nessuna possibilità di inserire PSK manuale o generarne una random.
- **Cosa serve**: Due modalità nel dialog, come su Android MeshCore:
  - **"Join Private Channel"**: nome + campo PSK manuale (hex, 16 byte)
  - **"Create Private Channel"**: nome + PSK random generata automaticamente
  - Il protocollo `CMD_SET_CHANNEL` (32) già supporta PSK arbitrarie (offset +34, 16 byte)
- **Difficoltà**: media
- **File**: AddChannelWindow.cpp/h, MainWindow.cpp

### S4. Cancellazione messaggi (singolo + clear chat) — [#5](https://github.com/atomozero/Sestriere/issues/5)
- **Segnalato da**: serwin2 (scotty3g)
- **Stato attuale**: nessuna funzionalità di cancellazione. Context menu ChatView ha solo "Copy" e "Save Image". DatabaseManager non ha metodi delete.
- **Cosa serve**:
  1. **Delete singolo messaggio**: voce "Delete" nel context menu di ChatView → `DatabaseManager::DeleteMessage(rowid)` → refresh ChatView
  2. **Clear all messages per contatto/canale**: voce "Clear Chat" nel context menu della sidebar → `DatabaseManager::DeleteMessagesForContact(key)` → `ChatView::ClearMessages()`
  3. Dialog di conferma prima di ogni cancellazione
- **Difficoltà**: media
- **File**: DatabaseManager.cpp/h, ChatView.cpp/h, MainWindow.cpp, Constants.h

### S5. Registrazione audio non funzionante — [#6](https://github.com/atomozero/Sestriere/issues/6)
- **Segnalato da**: serwin2 (scotty3g)
- **Problema**: click sul microfono mostra BAlert "Could not start recording. Check that a microphone is connected." nonostante SoundRecorder funzioni correttamente sullo stesso sistema. Non è solo un problema di feedback — è un **bug reale** nell'AudioEngine.
- **Analisi**: `AudioEngine` usa `BMediaRecorder` con auto-connect al system audio input. Se l'auto-connect fallisce (nodo non trovato, formato non compatibile), `StartRecording()` ritorna errore. SoundRecorder probabilmente usa un approccio diverso per agganciare l'input audio. Possibili cause:
  1. `BMediaRecorder::Connect()` non trova il nodo input corretto
  2. Il formato richiesto (8kHz mono 16-bit) non è supportato dal nodo sorgente senza conversione
  3. Il media_server ha il nodo input bloccato da un altro consumer
- **Fix**: investigare il flusso di connessione in `AudioEngine::StartRecording()`. Provare approccio alternativo: usare `BMediaRoster::GetAudioInput()` per ottenere il nodo di sistema, oppure accettare il formato nativo del nodo e fare resampling in software (il resampling nel callback esiste già).
- **Difficoltà**: media — richiede debug su Haiku con media_server attivo
- **File**: AudioEngine.cpp/h, MainWindow.cpp

---

## Gap conformità protocollo (wiki MeshCore vs implementazione)

### G1. `CMD_SET_RADIO_PARAMS` — manca `repeat_mode` (v9+)
- **Wiki**: byte aggiuntivo `repeat_mode` (boolean, abilita client-repeat off-grid mode)
- **Sestriere**: invia 11 byte (code + freq + bw + sf + cr), senza repeat_mode
- **Impatto**: impossibile abilitare/disabilitare client-repeat mode dalla UI
- **Fix**: aggiungere byte [11] = repeat_mode al frame. Checkbox "Client Repeat" in SettingsWindow radio tab.
- **Difficoltà**: bassa
- **File**: ProtocolHandler.cpp (`SendRadioParams`), SettingsWindow.cpp

### G2. `TXT_TYPE_SIGNED_PLAIN` (2) — non gestito
- **Wiki**: `TXT_TYPE_SIGNED_PLAIN = 2` — plain text firmato dal sender
- **Sestriere**: definisce solo `TXT_TYPE_PLAIN` (0) e `TXT_TYPE_CLI_DATA` (1). Messaggi firmati ricevuti da altri client non riconosciuti.
- **Fix**: aggiungere costante. In ricezione, trattare come plain text con indicatore "signed" nel bubble. L'invio può attendere.
- **Difficoltà**: bassa
- **File**: Constants.h, MainWindow.cpp (handler ricezione messaggi)

### G3. `PUSH_LOGIN_SUCCESS` — campi non parsati — COMPLETATO
- **Wiki**: frame contiene `permissions` (byte), `pub_key_prefix` (6 byte), `tag` (int32), `new_permissions` (byte, V7+)
- **Fix**: handler ora parsa tutti i campi. `permissions` bit 0 → `fLoggedInAsAdmin`. Log mostra admin/guest, prefix, new_permissions (V7+). Short frame gestiti con fallback.
- **Stato**: completato (commit edc2d27)

### G4. `ERR_CODE_*` — codici errore non decodificati — COMPLETATO
- **Wiki**: 6 codici errore definiti (UNSUPPORTED_CMD=1, NOT_FOUND=2, TABLE_FULL=3, BAD_STATE=4, FILE_IO_ERROR=5, ILLEGAL_ARG=6)
- **Fix**: aggiunte costanti `ERR_CODE_*` + funzione `ErrorCodeToString()` in Constants.h. `_HandleCmdErr()` ora logga messaggi leggibili. Sostituito magic number `2` con `ERR_CODE_NOT_FOUND` nell'enumerazione canali.
- **Stato**: completato (commit cae2e7b)

### G5. `PUSH_RAW_RADIO_PACKET` (0x88) — non nella wiki
- **Sestriere**: implementa e gestisce `PUSH_RAW_RADIO_PACKET = 0x88` nel PacketAnalyzer e MainWindow
- **Wiki**: codice 0x88 non documentato. Potrebbe essere un'estensione custom o rimosso dalla spec.
- **Azione**: verificare con il firmware attuale se è ancora supportato. Se deprecato, segnare come legacy.
- **Difficoltà**: nessuna (solo verifica)

### G6. `CMD_GET_CHANNEL` (31) / `CMD_SET_CHANNEL` (32) / `RSP_CHANNEL_INFO` (18) — non nella wiki
- **Sestriere**: usa questi comandi per enumerazione e gestione canali. Funzionano correttamente.
- **Wiki**: non documentati. Protocollo canali potrebbe cambiare senza preavviso.
- **Azione**: segnalare alla community MeshCore per aggiornamento wiki. Monitorare release firmware.

### G7. Campo `attempt` in `CMD_SEND_TXT_MSG` — sempre 0
- **Wiki**: `attempt: byte, values: 0..3` per retry a livello protocollo
- **Sestriere**: invia sempre `attempt = 0`
- **Impatto**: nessun retry protocol-level. Correlato a feature F2 (retry con backoff).
- **Fix**: quando si implementa F2, incrementare `attempt` ad ogni retry.
- **Difficoltà**: legata a F2

### G8. `CMD_GET_CONTACTS` — parametro `since` non utilizzato
- **Wiki**: parametro opzionale `since: uint32` per sync incrementale. `RSP_END_OF_CONTACTS` restituisce `most_recent_lastmod` da usare come `since` successivo.
- **Sestriere**: invia sempre `CMD_GET_CONTACTS` senza `since`, forzando full sync ogni volta
- **Impatto**: inefficiente con molti contatti
- **Fix**: salvare `most_recent_lastmod` da `RSP_END_OF_CONTACTS`, passarlo come `since` nelle sync successive. Mantenere full sync come fallback (es. primo avvio, cambio companion).
- **Difficoltà**: bassa
- **File**: MainWindow.cpp, ProtocolHandler.cpp (`SendGetContacts`)

---

## Feature esistenti dalla roadmap precedente (non completate)

### P2. Custom Variables UI (GET/SET_CUSTOM_VARS)
- **Dove**: protocollo implementato, risposta loggata, nessuna interfaccia
- **Cosa fare**: tab "Custom Variables" in SettingsWindow con lista chiave/valore
- **Sforzo**: medio

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
- **Correlato**: G7 (campo `attempt` nel protocollo)
- **Difficoltà**: media-alta
- **File**: MainWindow.cpp (timer management), Types.h (delivery status), ProtocolHandler.cpp

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

---

## UX / Persistenza

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

## Copertura protocollo

### Comandi e risposte

| Categoria | Implementati | Wiki | Note |
|-----------|-------------|------|------|
| CMD_* (inbound) | 40 | 35 documentati | +5 canali non documentati (GET/SET_CHANNEL, etc.) |
| RSP_* (outbound) | 22 | ~20 documentati | +RSP_CHANNEL_INFO (18) non documentato |
| PUSH_* (notifiche) | 15 | 13 documentati | +0x88 RAW_RADIO_PACKET non documentato |

### Gap conformità attivi

| ID | Gap | Priorità | Stato |
|----|-----|----------|-------|
| G1 | repeat_mode mancante in SET_RADIO_PARAMS | Media | Da fare |
| G2 | TXT_TYPE_SIGNED_PLAIN non gestito | Media | Da fare |
| G3 | Login success campi non parsati | Media | Completato |
| G4 | ERR_CODE non decodificati | Bassa | Completato |
| G5 | 0x88 non documentato nella wiki | Bassa | Da verificare |
| G6 | Channel commands non documentati | Bassa | Monitorare |
| G7 | attempt sempre 0 | Media | Legato a F2 |
| G8 | since param non usato in GET_CONTACTS | Bassa | Da fare |

---

## Priorità complessiva e ordine di implementazione

### Sprint 1 — Bug critici UX (v2.0)

| ID | Issue | Tipo | Descrizione | Difficoltà |
|----|-------|------|-------------|------------|
| S1 | [#2](https://github.com/atomozero/Sestriere/issues/2) | Bug | Doppio `#` nei nomi canali hashtag | ~~Bassa~~ DONE |
| S2 | [#3](https://github.com/atomozero/Sestriere/issues/3) | Bug | Public Channel duplicato/non funzionante | ~~Media~~ DONE |
| S5 | [#6](https://github.com/atomozero/Sestriere/issues/6) | Bug | Registrazione audio non funzionante | Media |
| G4 | — | Proto | ERR_CODE decodifica human-readable | ~~Bassa~~ DONE |
| G3 | — | Proto | Login success parsing completo | ~~Bassa~~ DONE |

### Sprint 2 — Feature utenti + conformità (v2.1)

| ID | Issue | Tipo | Descrizione | Difficoltà |
|----|-------|------|-------------|------------|
| S4 | [#5](https://github.com/atomozero/Sestriere/issues/5) | Feature | Cancellazione messaggi (singolo + clear chat) | Media |
| S3 | [#4](https://github.com/atomozero/Sestriere/issues/4) | Feature | PSK custom/random per canali privati | Media |
| G1 | — | Proto | repeat_mode in SET_RADIO_PARAMS | Bassa |
| G2 | — | Proto | TXT_TYPE_SIGNED_PLAIN gestione ricezione | Bassa |
| G8 | — | Proto | Sync incrementale contatti (param since) | Bassa |

### Sprint 3 — Resilienza messaggi (v2.2)

| ID | Tipo | Descrizione | Difficoltà |
|----|------|-------------|------------|
| F2+G7 | Feature | Retry messaggi con backoff + campo attempt | Media-Alta |
| F3 | Feature | Coda messaggi offline | Media |
| F1 | Feature | Compressione SMAZ | Media |

### Sprint 4 — Polish e completamento (v2.3+)

| ID | Tipo | Descrizione | Difficoltà |
|----|------|-------------|------------|
| P2 | Feature | Custom Variables UI | Media |
| F4 | Feature | Download bulk tile mappa | Media |
| U1 | UX | Persistere zoom/pan mappa | Bassa |
| U2 | UX | Persistere larghezza sidebar | Bassa |
| U3 | UX | VACUUM periodico DB | Bassa |
| U4 | UX | Validazione tile cache | Bassa |
| U5 | UX | Admin multi-repeater | Media |

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
| Media display max | 250x300 px | ChatView |
| MQTT publish interval | ~10 secondi | MqttClient.cpp |
| Memory tile cache | 32 tiles | TileCache.h |
| Pruning SNR history | 30 giorni | DatabaseManager.cpp |

---

## Riferimenti

- MeshCore Companion Protocol Wiki: https://github.com/ripplebiz/MeshCore/wiki/Companion-Radio-Protocol
- meshcore-open: https://github.com/zjs81/meshcore-open
- SMAZ algorithm: https://github.com/antirez/smaz
- Open-Meteo Elevation API: https://open-meteo.com/en/docs/elevation-api
- CayenneLPP format: https://docs.mydevices.com/docs/lorawan/cayenne-lpp
- OSM tile server: https://tile.openstreetmap.org/{z}/{x}/{y}.png
