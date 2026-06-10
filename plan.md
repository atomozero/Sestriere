# Sestriere — Piano di Hardening Corporate

Analisi completa del progetto con 5 audit paralleli (Memory/Thread Safety, Protocol, UI/UX, Database, Build System).
Totale: ~70 issue identificate, organizzate per priorita' e fase di implementazione.

---

## FASE 1 — Bug Critici e Sicurezza (da fixare subito)

### 1.1 Integer underflow nel parsing messaggi (CRASH/OOB)
- **File:** MainWindow.cpp:4681, 4918
- **Bug:** `size_t textLen = length - textOffset` — se `length < textOffset`, underflow unsigned → memcpy OOB
- **Fix:** Aggiungere `if (length <= textOffset) { textLen = 0; }` prima della sottrazione
- **Impatto:** Potenziale crash su frame malformati ricevuti via radio

### 1.2 gmtime() non rientrante in MqttClient (thread safety)
- **File:** MqttClient.cpp:298
- **Bug:** `gmtime()` restituisce puntatore statico; il thread mosquitto puo' sovrascriverlo
- **Fix:** Sostituire con `gmtime_r(&now, &tmBuf)`

### 1.3 Race condition su fConnected in MqttClient
- **File:** MqttClient.cpp:77-84, 244, 293
- **Bug:** `fConnected` letto senza lock in PublishStatus/PublishPacket, scritto dal callback thread
- **Fix:** Wrappare i metodi pubblici con `BAutolock lock(fStateLock)`

### 1.4 AudioEngine callback race condition
- **File:** AudioEngine.cpp:194-278
- **Bug:** `_RecordBuffer` scrive su fRecordBuffer/fRecordSize dal thread media senza lock; StopRecording() legge dallo UI thread
- **Fix:** Aggiungere BLocker per proteggere accessi condivisi

### 1.5 Voice PCM buffer use-after-free
- **File:** MainWindow.cpp:8375-8391
- **Bug:** `fVoicePlayPcm` passato a AudioEngine::Play() che lo legge in callback asincrono; il buffer puo' essere deletato prima che la callback finisca
- **Fix:** Copiare il buffer dentro AudioEngine oppure garantire lifetime con reference counting

### 1.6 sqlite3_close() non sicuro durante query attive
- **File:** DatabaseManager.cpp:116-124
- **Bug:** `sqlite3_close()` fallisce se ci sono statement aperti
- **Fix:** Usare `sqlite3_close_v2()` che differisce la chiusura

---

## FASE 2 — Robustezza Protocollo e Database

### 2.1 Bounds check mancanti nei handler protocollo
- **File:** MainWindow.cpp — vari handler
- `_HandleChannelInfo`: accede offset senza check intermedi (righe 5316-5378)
- `_HandleExportContact`: minLength troppo basso (3 vs 35 necessari) (riga 4170)
- `_HandleStats`: accessi a offset prima del bounds check (righe 5205-5260)
- **Fix:** Aggiungere `if (length < N) return;` prima di ogni accesso a data[N-1]

### 2.2 BMessageRunner double-delete
- **File:** MainWindow.cpp:1594, 1619, 413-419
- **Bug:** `delete fStatsRefreshTimer` senza `= NULL` dopo; il distruttore le ri-deleta
- **Fix:** Sempre `delete ptr; ptr = NULL;` per tutti i BMessageRunner

### 2.3 SerialHandler fTarget race condition
- **File:** SerialHandler.cpp:493-502
- **Bug:** `fTarget` controllato per NULL poi usato; ma tra check e uso puo' cambiare
- **Fix:** Copiare il puntatore in locale sotto lock prima di usarlo

### 2.4 PRAGMA busy_timeout mancante
- **File:** DatabaseManager.cpp:83-85
- **Bug:** Senza busy_timeout, le query ritornano SQLITE_BUSY immediatamente sotto carico
- **Fix:** Aggiungere `_Execute("PRAGMA busy_timeout=5000")` dopo WAL

### 2.5 Foreign keys abilitate solo in DeleteGroup()
- **File:** DatabaseManager.cpp:944-945
- **Bug:** `PRAGMA foreign_keys = ON` solo in una funzione; le cascade non funzionano altrove
- **Fix:** Spostare in Open() dopo l'apertura del DB

### 2.6 AddContactToGroup() non atomico
- **File:** DatabaseManager.cpp:963-1000
- **Bug:** DELETE + INSERT senza transaction; stato inconsistente possibile
- **Fix:** Wrappare in `BEGIN TRANSACTION` / `COMMIT`

### 2.7 sqlite3_step() non controllato nelle DELETE di pruning
- **File:** DatabaseManager.cpp:505, 513, 538, 643, 744, 825
- **Bug:** Return code ignorato; pruning silenziosamente fallisce
- **Fix:** Controllare rc e loggare errori

### 2.8 Schema migration senza error check ne' versioning
- **File:** DatabaseManager.cpp:94-99
- **Bug:** ALTER TABLE con errori ignorati; nessun sistema di versioning schema
- **Fix:** Creare tabella `schema_version`; controllare errori non-idempotenti

### 2.9 GIF download thread non joinato
- **File:** MainWindow.cpp:7593-7603
- **Bug:** Thread spawned ma mai `wait_for_thread()` nel distruttore
- **Fix:** Salvare thread_id e fare join nel distruttore

### 2.10 Child window creation during quit
- **File:** MainWindow.cpp:3119-3200
- **Bug:** Messaggi in arrivo possono creare finestre durante QuitRequested()
- **Fix:** Flag `fQuitting = true` all'inizio di QuitRequested(); check prima di creare finestre

---

## FASE 3 — Database Performance e Integrita'

### 3.1 Indici mancanti su colonne frequentemente cercate
- **File:** DatabaseManager.cpp schema
- `contact_group_members.contact_key` — nessun indice
- `topology_edges.timestamp` — nessun indice per pruning
- **Fix:** `CREATE INDEX IF NOT EXISTS idx_group_members_contact ON contact_group_members(contact_key)`

### 3.2 Timestamp 32-bit (Y2038)
- **File:** DatabaseManager.cpp — multipli
- **Bug:** Cast a `(int)time(NULL)` perde precisione; overflow nel 2038
- **Fix:** Usare `sqlite3_bind_int64()` per i timestamp

### 3.3 Rollback mancante nella migrazione bulk
- **File:** DatabaseManager.cpp:1292-1351
- **Bug:** Se InsertMessage() fallisce durante import, il COMMIT salva dati parziali
- **Fix:** Tracciare errori e fare ROLLBACK se qualcuno fallisce

### 3.4 No VACUUM periodico
- **File:** DatabaseManager.cpp
- **Bug:** Dopo heavy DELETE (pruning), il file DB cresce indefinitamente
- **Fix:** `PRAGMA optimize` alla chiusura; VACUUM opzionale periodico

---

## FASE 4 — Qualita' UI/UX per Look Corporate

### 4.1 Colori hardcoded non theme-aware
- **MapView.cpp**: background `{30, 40, 60}`, land `{60, 75, 55}` — non si adattano a tema chiaro
- **PacketAnalyzerWindow.cpp**: zone SNR con colori fissi light/dark
- **MessageView.cpp**: SAR marker border nero hardcoded
- **ContactItem.cpp**: badge text bianco hardcoded
- **Fix:** Usare `ui_color()` + `tint_color()` ovunque; helper per luminosita' background

### 4.2 Font size hardcoded
- TopBarView: `SetSize(9)`, `SetSize(11)`
- ContactInfoPanel: `SetSize(14)`
- ContactItem: `SetSize(9)` per badge
- **Fix:** Definire costanti `kFontSizeSmall/Normal/Large` in Constants.h; scalare con font di sistema

### 4.3 Stringhe inglesi hardcoded (preparazione i18n)
- Tooltip, label, messaggi di stato tutti in inglese inline
- **Fix:** Centralizzare in `Strings.h`; preparare per gettext futuro

### 4.4 Window Show/Activate senza LockLooper
- **File:** MainWindow.cpp:1026, 2322, 2333, 2344
- **Bug:** Pattern inconsistente; alcuni posti usano LockLooper, altri no
- **Fix:** Helper `_SafeShowWindow(BWindow*)` usato ovunque

### 4.5 Feedback utente mancante per operazioni lunghe
- Sync contacts, Ping All, export — nessun indicatore di progresso
- **Fix:** Status text nella TopBar o progress indicator

### 4.6 Click target troppo piccoli (accessibilita')
- TopBar icon: ~24x24px, WCAG raccomanda 44x44px
- **Fix:** Espandere padding orizzontale; area cliccabile piu' grande dell'icona

---

## FASE 5 — Architettura e Manutenibilita'

### 5.1 MainWindow.cpp monolitico (8400+ righe)
- Gestisce: UI, protocollo, messaggi, MQTT, telemetria, voice, immagini, repeater, delivery queue
- **Fix:** Estrarre in classi dedicate:
  - `ProtocolParser` — parsing frame e dispatch handler
  - `ChatController` — delivery queue, retry, messaggi
  - `SessionManager` — voice/image session lifecycle
  - `RepeaterController` — logica repeater admin

### 5.2 MapNode struct duplicato
- `NetworkMapWindow.h` ha `MapNode`, `MapView.h` ha `GeoMapNode` (rinominato per evitare collisione)
- **Fix:** Unificare in Types.h

### 5.3 Costanti UI duplicate tra file
- `kMargin`, `kAvatarSize`, spacing — definiti localmente in ogni .cpp
- **Fix:** Creare sezione UI Constants in Constants.h

### 5.4 CoastlineData.h — 12KB di dati nel header
- Array float[] incluso in ogni translation unit
- **Fix:** Spostare in .cpp con extern, o comprimere come int16

### 5.5 Versione non centralizzata
- Solo in Sestriere.rdef (1.1.0); non accessibile dal codice
- **Fix:** Definire `kAppVersion` in Constants.h; sincronizzare con .rdef

### 5.6 Test runner automatizzato mancante
- 35 test programs in tests/ ma nessun `make test` target
- **Fix:** Aggiungere target test nel Makefile; script runner

---

## Riepilogo Priorita'

| Fase | Issue | Severita' | Effort |
|------|-------|-----------|--------|
| 1 | 6 fix | CRITICA | 1-2 sessioni |
| 2 | 10 fix | ALTA | 2-3 sessioni |
| 3 | 4 fix | MEDIA | 1 sessione |
| 4 | 6 fix | MEDIA | 2-3 sessioni |
| 5 | 6 refactor | BASSA | 4-5 sessioni |

**Raccomandazione:** Procedere con Fase 1 immediatamente (bug critici di sicurezza),
poi Fase 2 (robustezza), poi Fase 4 (polish UI) per il prossimo rilascio.
Le Fasi 3 e 5 possono essere pianificate come miglioramenti continui.
