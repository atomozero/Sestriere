# Sestriere — Roadmap

Analisi dello stato attuale e piano di sviluppo futuro.
Ultimo aggiornamento: marzo 2026 (v2.0-dev)

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

### 15. Airtime Display & Nearest Repeater
TX/RX airtime (secondi) parsato da radio stats e mostrato nella top bar (`AT:Xs/Ys`).
Nearest repeater calcolato con distanza haversine GPS e mostrato come `>>Nome X.Xkm`.
Tooltip con dettagli al hover. Ricalcolato dopo ogni contact sync.

### 16. Message Reactions
Emoji reactions sui messaggi, compatibili con meshcore-open (formato `r:HASH:INDEX`).
Hash calcolato con algoritmo Dart VM (Jenkins-like) su `timestamp+senderName+first5chars`.
Context menu "React" con 6 quick emoji. Reactions ricevute matchate ai messaggi per hash
e visualizzate come contatori emoji. Reactions.h header-only con tabella 69 emoji.

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

### B14. Room messages sent as TXT_TYPE_CLI_DATA — v2.0-dev
Messaggi normali di chat a Room/Repeater inviati con `TXT_TYPE_CLI_DATA` (1) invece di
`TXT_TYPE_PLAIN` (0). Il server li interpretava come comandi admin, non come messaggi.
Fix: `_SendTextMessage()` usa sempre `TXT_TYPE_PLAIN`; il tipo CLI resta solo in `_SendCliCommand()`.

### B15. Room forwarded messages show Room name, not sender — v2.0-dev
I messaggi inoltrati da un Room arrivano con il pubkey del Room come mittente. Il testo
contiene `"SenderNick: messaggio"` ma Sestriere mostrava il nome del Room nel bubble.
Fix: per contatti type==3, il testo viene parsato per estrarre il nick prima del `:` e
mostrarlo come senderName nel bubble. Il testo visualizzato è solo la parte dopo `": "`.

### B16. Sestriere.rdef version out of sync con Constants.h — v2.1.1
- **Trovato da**: code review v2.1.1
- **Dove**: `src/Sestriere.rdef` vs `src/Constants.h`
- **Problema**: `app_version` nel rdef aveva `middle = 0, minor = 0` (corrispondente a 2.0.0) mentre Constants.h definiva `APP_VERSION "2.1.0"` con `MIDDLE=1, MINOR=0`. Il rdef non era stato aggiornato durante il bump a v2.1.0.
- **Impatto**: Haiku's `AboutWindow` e `listattr` mostravano versione 2.0.0 anziché 2.1.0.
- **Fix**: allineato rdef a `middle = 1, minor = 1` nel diff v2.1.1.
- **Stato**: fix in corso (diff v2.1.1, non ancora committato)

### B17. Password cap era stato erroneamente alzato a 16 — v2.1.1
- **Trovato da**: code review v2.1.1
- **Dove**: `src/ProtocolHandler.cpp` — `SendLogin()`, `src/LoginWindow.cpp` — `SetMaxBytes()`
- **Problema**: il cap era stato portato da 15 a 16 credendo che `MAX_ADMIN_PASS_LEN = 16` significasse 16 char utilizzabili. Verifica nel firmware MeshCore ha rivelato: `char password[16]` + `StrHelper::strncpy` null-terminante = **15 char utili**. Il firmware stesso tronca a 15 in `BaseChatMesh::sendLogin()`. La wiki conferma "max 15 bytes".
- **Impatto**: con cap 16, una password di 16 char veniva troncata silenziosamente dal firmware → login fallito.
- **Fix**: ripristinato cap a 15 in `ProtocolHandler.cpp` e `SetMaxBytes(15)` in `LoginWindow.cpp`.
- **Stato**: completato

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

### S3. PSK custom/random per canali privati — [#4](https://github.com/atomozero/Sestriere/issues/4) — COMPLETATO
- **Segnalato da**: serwin2 (scotty3g)
- **Fix**: AddChannelWindow ridisegnata con radio buttons "Create new channel" (PSK random) e "Join existing channel" (PSK hex manuale). MSG_CREATE_CHANNEL supporta tre sorgenti PSK: hex utente, random, o fallback SHA-256.
- **Stato**: completato (commit 2fb0e6b)

### S4. Cancellazione messaggi (singolo + clear chat) — [#5](https://github.com/atomozero/Sestriere/issues/5) — COMPLETATO
- **Segnalato da**: serwin2 (scotty3g)
- **Fix**: `DatabaseManager::DeleteMessage()` e `DeleteMessagesForContact()`. "Delete" nel context menu ChatView, "Clear Chat" nei context menu sidebar per canali e contatti. Rimozione da DB, liste in-memory e chat view.
- **Stato**: completato (commit e36f8dc)

### S5. Registrazione audio non funzionante — [#6](https://github.com/atomozero/Sestriere/issues/6) — COMPLETATO
- **Segnalato da**: serwin2 (scotty3g)
- **Problema**: click sul microfono mostra BAlert "Could not start recording. Check that a microphone is connected." nonostante SoundRecorder funzioni correttamente sullo stesso sistema.
- **Causa root**: `BMediaRecorder::Connect(format)` (overload a 1 argomento) connette al **mixer audio**, non all'input. Il mixer espone output in formato nativo (48kHz stereo float32) che non è compatibile con 8kHz mono int16 → fallisce con `B_MEDIA_BAD_SOURCE`. Inoltre il callback `_RecordBuffer` assumeva sempre dati int16, ma l'hardware produce float32.
- **Fix**: (1) `StartRecording()` ora usa `GetAudioInput()` + `GetFreeOutputsFor()` + overload `Connect(node, &output, &format)` a 3 argomenti, come fa SoundRecorder. (2) `SetAcceptedFormat()` con wildcard per accettare qualsiasi formato. (3) `_RecordBuffer()` riscritto con conversione format-aware (float32/int32/int16/uint8/int8) + resampling + downmix.
- **Stato**: completato (v2.1.1)

### S6. Verifica duplicati e tipo canale alla creazione — [#7](https://github.com/atomozero/Sestriere/issues/7)
- **Segnalato da**: serwin2
- **Problema**: si possono creare canali con nomi duplicati. I canali pubblici hashtag creati via "Create new channel" generano chiavi random anziché SHA-256 del nome. Serve distinguere: canale privato nuovo (key random), join canale privato esistente (key manuale), join canale hashtag (key SHA-256).
- **File**: AddChannelWindow.cpp, MainWindow.cpp
- **Difficoltà**: media

### S7. Login repeater causa scomparsa contatti — [#8](https://github.com/atomozero/Sestriere/issues/8) — COMPLETATO
- **Segnalato da**: serwin2
- **Problema**: dopo login repeater, i contatti spariscono dalla lista lasciando solo i canali.
- **Causa root**: due bug nel sync contatti:
  1. `_HandlePushLoginResult()` usava `SendGetContacts(fContactsSince)` — sync incrementale post-login. Se nessun contatto è cambiato, il device restituisce 0 contatti, ma `_HandleContactsStart()` aveva già svuotato `fContacts` in `fOldContacts` → lista vuota.
  2. `_HandleContactsEnd()` cancellava `fOldContacts.MakeEmpty()` i contatti non matchati, anche durante sync incrementale dove i non-restituiti sono semplicemente invariati.
- **Fix**: (1) forzare full sync (`since=0`) dopo ogni login. (2) in `_HandleContactsEnd()`, rimettere i contatti da `fOldContacts` in `fContacts` anziché cancellarli, così i contatti invariati sono preservati durante sync incrementale.
- **Stato**: completato (v2.1.1)

### S8. Contatti spariscono dopo autenticazione room/repeater — [#9](https://github.com/atomozero/Sestriere/issues/9) — COMPLETATO
- **Segnalato da**: PaxForever
- **Problema**: all'inserimento password per repeater/room, i contatti spariscono.
- **Nota**: stessa causa root di S7 — risolto dalla stessa fix.
- **Stato**: completato (v2.1.1)

### S9. Collegamento USB a volte non riconosciuto — [#10](https://github.com/atomozero/Sestriere/issues/10)
- **Segnalato da**: PaxForever
- **Problema**: la connessione USB a volte non si connette automaticamente. Il device potrebbe non essere riconosciuto; disconnettere, fare refresh porte e riconnettere a una porta USB appena rilevata risolve temporaneamente.
- **File**: SerialHandler.cpp, MainWindow.cpp (port enumeration)
- **Difficoltà**: media

### S10. Telemetry page non scorre con molti device — [#11](https://github.com/atomozero/Sestriere/issues/11)
- **Segnalato da**: PaxForever
- **Problema**: con più dispositivi che riportano telemetria, dopo 3-4 device visualizzati, la pagina non scorre verso il basso per mostrare dati aggiuntivi.
- **File**: TelemetryWindow.cpp
- **Difficoltà**: bassa

### S11. Network Map valori dB anomali — [#12](https://github.com/atomozero/Sestriere/issues/12)
- **Segnalato da**: PaxForever
- **Problema**: la Network Map mostra valori dB atipici quando tutti i dispositivi sono connessi (es. repeater a 53dB con companion a 12dB, room/repeater a 48dB con companion a 0dB). I valori potrebbero essere non firmati o parsati come unsigned.
- **File**: NetworkMapWindow.cpp
- **Difficoltà**: media

### S12. Room: simboli prima del testo messaggi — [#13](https://github.com/atomozero/Sestriere/issues/13) — COMPLETATO
- **Segnalato da**: PaxForever
- **Problema**: i messaggi nelle Room mostrano 3-4 simboli prima del testo.
- **Causa root**: SMAZ compression attiva su messaggi inviati a room (tipo 3). Il prefisso `s:` + bytes compressi appaiono come simboli sui client riceventi (stock MeshCore o Sestriere che decomprime ma mostra il nick room corrotto).
- **Fix**: disabilitata SMAZ compression per contatti tipo 2 (repeater) e 3 (room), come già fatto per i canali nel commit 5167e8d. SMAZ resta attivo solo per DM peer-to-peer.
- **Stato**: completato (v2.1.1)

### S13. Messaggi nelle room/chat private appaiono criptati — [#14](https://github.com/atomozero/Sestriere/issues/14) — COMPLETATO
- **Segnalato da**: PaxForever
- **Problema**: messaggi inviati da Haiku a room o chat dirette appaiono criptati/illeggibili sugli altri dispositivi.
- **Causa root**: stessa di S12 — SMAZ compression inviava `s:` + dati compressi binari. I client stock MeshCore (app Android, etc.) non supportano SMAZ e mostrano il contenuto compresso come testo corrotto.
- **Fix**: stessa fix di S12.
- **Stato**: completato (v2.1.1)

### S14. Gestione region mancante — [#15](https://github.com/atomozero/Sestriere/issues/15)
- **Segnalato da**: PaxForever
- **Problema**: non è possibile configurare la regione radio né per i canali né per il dispositivo nell'applicazione.
- **File**: SettingsWindow.cpp, ProtocolHandler.cpp
- **Difficoltà**: media

### S15. Manca comando "Imposta percorso" — [#16](https://github.com/atomozero/Sestriere/issues/16)
- **Segnalato da**: PaxForever
- **Problema**: esiste "Reset path" per ripristinare i percorsi di rete ma manca il comando per impostare inizialmente i percorsi. La selezione del percorso determina i repeater preferiti per il routing dei pacchetti.
- **File**: MainWindow.cpp (context menu, protocol commands)
- **Difficoltà**: media

---

## Gap conformità protocollo (wiki MeshCore vs implementazione)

### G1. `CMD_SET_RADIO_PARAMS` — manca `repeat_mode` (v9+) — COMPLETATO
- **Fix**: aggiunto byte [11] = repeat_mode al frame. Default false per compatibilità.
- **Stato**: completato (commit f97fe68)

### G2. `TXT_TYPE_SIGNED_PLAIN` (2) — non gestito — COMPLETATO
- **Fix**: aggiunta costante `TXT_TYPE_SIGNED_PLAIN = 2`. Messaggi firmati già accettati in ricezione.
- **Stato**: completato (commit f97fe68)

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

### G8. `CMD_GET_CONTACTS` — parametro `since` non utilizzato — COMPLETATO
- **Fix**: `SendGetContacts(uint32 since)` con parametro opzionale. `fContactsSince` salvato da `RSP_END_OF_CONTACTS`. Full sync su connessione iniziale, incremental per sync successivi. Reset a 0 su disconnect.
- **Stato**: completato (commit 16aa42a)

---

## Feature esistenti dalla roadmap precedente (non completate)

### P2. Custom Variables UI (GET/SET_CUSTOM_VARS) — COMPLETATO
- **Fix**: tab "Variables" in SettingsWindow con lista key:value, campi edit Name/Value, pulsanti Set e Refresh. Variables caricate automaticamente all'apertura. MSG_GET_CUSTOM_VARS e MSG_SET_CUSTOM_VAR con forward alla SettingsWindow.
- **Stato**: completato (commit d690969)

### F1. Compressione SMAZ per messaggi — COMPLETATO
- **Cosa**: compressione dizionario per testo chat (30-50% risparmio)
- **Fix**: Smaz.h header-only con algoritmo SMAZ portato da antirez/smaz. Compressione automatica in invio DM e canale quando il risultato è più corto, prefisso `s:` per compatibilità meshcore-open. Decompressione trasparente in ricezione. Messaggi speciali (GIF, voice, image, CLI) esclusi.
- **Stato**: completato (commit efdd5b6)

### F2. Retry messaggi con backoff esponenziale — COMPLETATO
- **Fix**: timeout backoff esponenziale 15s→30s→60s (era fisso 60s). Campo `attempt` (0-3) ora inviato nel frame CMD_SEND_TXT_MSG per deduplicazione lato radio. Il sistema di retry (3 tentativi, indicatore RETRYING nel bubble, FAILED dopo esaurimento) era già implementato.
- **Stato**: completato (commit b050af4)

### F3. Coda messaggi offline — COMPLETATO
- **Fix**: messaggi DM inviabili anche offline, salvati con status PENDING in DB e in-memory queue. `_DrainOutbox()` invia i messaggi in coda dopo la riconnessione e il sync contatti. Messaggi PENDING preservati alla disconnessione (non più marcati FAILED). Il delivery timer ignora i messaggi con attemptCount=0 (non ancora inviati). Canali richiedono ancora connessione attiva.
- **Stato**: completato (commit 2608e48)

### F4. Download bulk tile mappa — COMPLETATO
- **Fix**: pulsante "Download Area" nella toolbar mappa scarica tutti i tile per l'area visibile corrente ai livelli zoom corrente ±1. Usa il TileCache async con LRU disk cache 50 MB.
- **Stato**: completato (commit 58f6eeb)

---

## UX / Persistenza

### U1. Persistere zoom/pan della mappa — GIÀ IMPLEMENTATO
- Già presente: `SaveMapState()`/`LoadMapState()` salvano center_lat, center_lon, zoom e tiles toggle in `map.settings`. Nessuna modifica necessaria.

### U2. Persistere larghezza sidebar e info panel
- **Dove**: MainWindow.cpp — BSplitView pesi
- **Fix**: salvare proporzioni in `ui.settings`, applicare dopo _BuildUI

### U3. VACUUM periodico del database — GIÀ IMPLEMENTATO
- Già presente: `PRAGMA auto_vacuum=INCREMENTAL` in Open() e `PRAGMA incremental_vacuum` dopo ogni PruneOldData(). Nessuna modifica necessaria.

### U4. Validazione tile cache corrotte — GIÀ IMPLEMENTATO
- Già presente: `_LoadFromDisk()` verifica magic bytes PNG, elimina tile corrotte e aggiorna accounting. Nessuna modifica necessaria.

### U5. Admin multi-repeater simultaneo — COMPLETATO
- **Fix**: sostituiti fLoggedIn/fLoggedInKey/fLoggedInAsAdmin con `OwningObjectList<AdminSession>` che traccia sessioni concorrenti. CLI invia al contatto selezionato, status accettati da qualsiasi sessione attiva, console mode per-contatto.
- **Stato**: completato (commit a5f50ed)

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
| G1 | repeat_mode mancante in SET_RADIO_PARAMS | Media | Completato |
| G2 | TXT_TYPE_SIGNED_PLAIN non gestito | Media | Completato |
| G3 | Login success campi non parsati | Media | Completato |
| G4 | ERR_CODE non decodificati | Bassa | Completato |
| G5 | 0x88 non documentato nella wiki | Bassa | Da verificare |
| G6 | Channel commands non documentati | Bassa | Monitorare |
| G7 | attempt sempre 0 | Media | Completato |
| G8 | since param non usato in GET_CONTACTS | Bassa | Completato |

---

## Priorità complessiva e ordine di implementazione

### Sprint 1 — Bug critici UX (v2.0)

| ID | Issue | Tipo | Descrizione | Difficoltà |
|----|-------|------|-------------|------------|
| S1 | [#2](https://github.com/atomozero/Sestriere/issues/2) | Bug | Doppio `#` nei nomi canali hashtag | ~~Bassa~~ DONE |
| S2 | [#3](https://github.com/atomozero/Sestriere/issues/3) | Bug | Public Channel duplicato/non funzionante | ~~Media~~ DONE |
| S5 | [#6](https://github.com/atomozero/Sestriere/issues/6) | Bug | Registrazione audio non funzionante | ~~Media~~ DONE |
| G4 | — | Proto | ERR_CODE decodifica human-readable | ~~Bassa~~ DONE |
| G3 | — | Proto | Login success parsing completo | ~~Bassa~~ DONE |

### Sprint 2 — Feature utenti + conformità (v2.1)

| ID | Issue | Tipo | Descrizione | Difficoltà |
|----|-------|------|-------------|------------|
| S4 | [#5](https://github.com/atomozero/Sestriere/issues/5) | Feature | Cancellazione messaggi (singolo + clear chat) | ~~Media~~ DONE |
| S3 | [#4](https://github.com/atomozero/Sestriere/issues/4) | Feature | PSK custom/random per canali privati | ~~Media~~ DONE |
| G1 | — | Proto | repeat_mode in SET_RADIO_PARAMS | ~~Bassa~~ DONE |
| G2 | — | Proto | TXT_TYPE_SIGNED_PLAIN gestione ricezione | ~~Bassa~~ DONE |
| G8 | — | Proto | Sync incrementale contatti (param since) | ~~Bassa~~ DONE |

### Sprint 3 — Resilienza messaggi (v2.2)

| ID | Tipo | Descrizione | Difficoltà |
|----|------|-------------|------------|
| F2+G7 | Feature | Retry messaggi con backoff + campo attempt | ~~Media-Alta~~ DONE |
| F3 | Feature | Coda messaggi offline | ~~Media~~ DONE |
| F1 | Feature | Compressione SMAZ | ~~Media~~ DONE |

### Sprint 4 — Polish e completamento (v2.3+)

| ID | Tipo | Descrizione | Difficoltà |
|----|------|-------------|------------|
| P2 | Feature | Custom Variables UI | ~~Media~~ DONE |
| F4 | Feature | Download bulk tile mappa | ~~Media~~ DONE |
| U1 | UX | Persistere zoom/pan mappa | ~~Bassa~~ GIÀ FATTO |
| U2 | UX | Persistere larghezza sidebar | ~~Bassa~~ DONE |
| U3 | UX | VACUUM periodico DB | ~~Bassa~~ GIÀ FATTO |
| U4 | UX | Validazione tile cache | ~~Bassa~~ GIÀ FATTO |
| U5 | UX | Admin multi-repeater | ~~Media~~ DONE |

### Sprint 5 — Bug utenti aprile 2026 (v2.2)

| ID | Issue | Tipo | Descrizione | Difficoltà | Priorità |
|----|-------|------|-------------|------------|----------|
| S7 | [#8](https://github.com/atomozero/Sestriere/issues/8) | Bug | Login repeater causa scomparsa contatti | ~~Media-Alta~~ DONE | **Critica** |
| S8 | [#9](https://github.com/atomozero/Sestriere/issues/9) | Bug | Contatti spariscono dopo auth room/repeater | ~~Media~~ DONE | **Critica** (correlato S7) |
| S13 | [#14](https://github.com/atomozero/Sestriere/issues/14) | Bug | Messaggi DM/room appaiono criptati su altri device | ~~Media-Alta~~ DONE | **Alta** |
| S12 | [#13](https://github.com/atomozero/Sestriere/issues/13) | Bug | Room: simboli prima del testo messaggi | ~~Media~~ DONE | **Alta** |
| S11 | [#12](https://github.com/atomozero/Sestriere/issues/12) | Bug | Network Map valori dB anomali | Media | Media |
| S10 | [#11](https://github.com/atomozero/Sestriere/issues/11) | Bug | Telemetry non scorre con molti device | Bassa | Media |
| S9 | [#10](https://github.com/atomozero/Sestriere/issues/10) | Bug | USB a volte non riconosciuto | Media | Media |
| S6 | [#7](https://github.com/atomozero/Sestriere/issues/7) | Feature | Verifica duplicati/tipo canale alla creazione | Media | Bassa |
| S14 | [#15](https://github.com/atomozero/Sestriere/issues/15) | Feature | Gestione region radio mancante | Media | Bassa |
| S15 | [#16](https://github.com/atomozero/Sestriere/issues/16) | Feature | Comando "Imposta percorso" mancante | Media | Bassa |

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
