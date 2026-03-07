# Sestriere — Roadmap Feature

Feature ispirate dall'analisi di meshcore-open, adattate per Haiku OS nativo.

---

## 1. Contact Groups + Channel Muting ✓ COMPLETATO

### Contact Groups ✓
Contatti organizzati in gruppi nominati via SQLite (`contact_groups` + `contact_group_members`).
- Right-click contatto → menu "Group" → seleziona/crea/rimuovi gruppo
- Sidebar mostra separatori per gruppo, contatti non raggruppati sotto "Ungrouped"
- Un contatto può appartenere a un solo gruppo alla volta
- Filtro testo si applica sia a nomi contatto che nomi gruppo

### Channel/Contact Muting ✓
Flag mute persistente in SQLite (`mute_settings`) per contatti e canali.
- Right-click → "Mute"/"Unmute" su qualsiasi contatto o canale
- Mute sopprime notifiche desktop e badge unread
- Nome visualizzato in colore attenuato quando muted
- Chiave mute: `aabbccddeeff` per contatti, `ch_public`/`ch_N` per canali

**File modificati**: Constants.h, ContactItem.cpp/h, MainWindow.cpp/h, DatabaseManager.cpp/h
**Test**: test_mute_logic (6 test), test_contact_groups (8 test)

---

## 2. GIF Animate via GIPHY ✓ COMPLETATO

GIF animate condivise tramite GIPHY, compatibili con meshcore-open.
- Invio: ricerca su GIPHY, selezione GIF, invio `g:{gifId}` come testo
- Ricezione: rilevamento prefisso `g:`, download da CDN GIPHY, animazione in chat
- GIF Picker con griglia animata (3 colonne, thumbnails animate)
- Cache locale in `~/config/settings/Sestriere/gif_cache/`
- Zero byte extra su LoRa — solo ID testuale trasmesso

**File**: GiphyClient.cpp/h, GifPickerWindow.cpp/h, ImageCodec.cpp/h (DecompressGifFrames), MessageView.cpp/h, ChatView.cpp/h, MainWindow.cpp/h, Constants.h
**Dipendenze**: curl_devel, giflib_devel

---

## 3. Image Sharing via LoRa ✓ COMPLETATO

Condivisione immagini tramite trasferimento chunked su LoRa.
- Compressione e invio a blocchi con ImageCodec
- Auto-fetch dei chunk mancanti
- Visualizzazione inline nella chat con ridimensionamento
- Integrazione con ImageSession per gestione sessioni

**File**: ImageCodec.cpp/h, ImageSession.cpp/h, MainWindow.cpp/h

---

## 4. SAR Markers ✓ COMPLETATO

Parsing e visualizzazione marker SAR (Search and Rescue) compatibili con meshcore-sar.
- Marker visualizzati nella chat con tipo, coordinate e descrizione
- Marker con GPS mostrati sulla mappa geografica
- Parser integrato nel flusso messaggi

**File**: SarMarker.cpp/h, MainWindow.cpp/h, MapView.cpp/h

---

## 5. Emoji Rendering ✓ COMPLETATO

Rendering emoji Unicode tramite sprite PNG con alpha compositing.
- Emoji riconosciute e sostituite con bitmap PNG
- Compositing trasparente su sfondo bolle chat
- Rendering corretto in light/dark theme

**File**: EmojiRenderer.cpp/h, MessageView.cpp/h

---

## 6. OSM Map Tiles ✓ COMPLETATO

Overlay tile OpenStreetMap sulla mappa geografica con cache offline.
- Download tile da server OSM con TileCache
- Cache locale in `~/config/settings/Sestriere/tiles/`
- Rendering coastline con dati poligonali

**File**: TileCache.cpp/h, CoastlineData.cpp/h, MapView.cpp/h

---

## 7. UI Settings Persistence ✓ COMPLETATO

Salvataggio e ripristino impostazioni UI (filtri contatti, etc.).
- Filtri Chat/Repeater/Room salvati in `ui.settings`
- Ripristinati automaticamente all'avvio

**File**: MainWindow.cpp/h

---

## 8. SMAZ Message Compression

Compressione dizionario per messaggi brevi, ottimizzata per chat.
- Dizionario di 254 pattern comuni ("the ", " of ", "ing", "tion", ecc.)
- Compressione tipica: 30-50% su messaggi inglesi
- Messaggio compresso prefissato con marker `s:` (compatibile meshcore-open)
- Funzione `encodeIfSmaller()`: applica solo se riduce effettivamente la dimensione
- Implementazione self-contained: ~150 righe C++ encode/decode + tabella dizionario

### Motivazione
LoRa ha budget dati strettissimo (max ~255 byte). Su EU 868 MHz c'e duty cycle 1% imposto per legge. Ogni byte risparmiato = piu testo utile o meno airtime.

### Compatibilita cross-client
Il marker `s:` deve coincidere con quello di meshcore-open. Verificare il formato esatto prima dell'implementazione.

**Difficolta**: Media
**File coinvolti**: nuovo Smaz.h/cpp, ProtocolHandler.cpp (integrazione invio), MainWindow.cpp (integrazione ricezione)

---

## 9. Message Retry con Exponential Backoff

Retry automatico dei messaggi quando PUSH_SEND_CONFIRMED non arriva.

### Meccanismo
1. Dopo invio, timer di 1 secondo
2. Se ACK non arriva, rimanda il messaggio
3. Backoff esponenziale: 1s, 2s, 4s, 8s, 16s
4. Dopo 5 tentativi: messaggio marcato "failed"
5. Opzionale: `SendResetPath()` dopo fallimento per forzare ricalcolo rotta

### Deduplicazione
- ACK hash: SHA256(timestamp + attempt + text + sender_pubkey) troncato
- History circolare delle ultime 100 entry
- Ricevente ignora messaggi con hash gia visto

### Motivazione
Attualmente se l'ACK non arriva il messaggio resta in stato "sent" per sempre. L'utente deve accorgersi e rimandare manualmente. Il retry automatico e la differenza tra "i messaggi arrivano" e "i messaggi a volte si perdono".

**Difficolta**: Media-Alta
**File coinvolti**: nuovo MessageRetryService.h/cpp (o integrato in MainWindow), Types.h (delivery status), MainWindow.cpp (timer management)

---

## 10. Offline Map Tiles

Pre-download dei tile mappa OSM per uso senza connessione internet.

### Flusso utente
1. Seleziona area sulla mappa (rettangolo)
2. Sceglie livelli di zoom (es. 10-15)
3. App calcola e scarica i tile PNG
4. Storage: `~/config/settings/Sestriere/tiles/z/x/y.png`
5. Mappa offline carica tile dalla cache locale

### Calcolo tile
Per ogni zoom level: `(x_max - x_min + 1) * (y_max - y_min + 1)` tile.
Area 50x50 km a zoom 10-15 = circa 20-50 MB.

### Decisione architetturale
- Opzione A: aggiungere tile rendering al MapView.cpp esistente (meno lavoro)
- Opzione B: creare mappa tile-based separata (piu pulita)

### Motivazione
Sestriere ha gia mappa geografica (MapView.cpp) con GPS dei contatti, ma richiede internet per i tile. In scenari off-grid (dove LoRa mesh ha piu senso) non c'e internet.

**Difficolta**: Media
**File coinvolti**: MapView.cpp/h (o nuovo TileMapView.cpp/h), nuovo MapTileCache.cpp/h, UI per selezione area e progress download

### Note tecniche Haiku
- Download HTTP: `BUrlRequest` dal Network Kit
- Rendering tile PNG: `TranslatorRoster` + `BBitmap`
- File I/O: standard POSIX o Haiku `BFile`/`BDirectory`

---

## 11. Line-of-Sight Analysis

Calcolo profilo elevazione terreno tra due punti per verificare se esiste linea di vista diretta per il segnale radio LoRa.

### Come funziona
1. Campionare 21-81 punti lungo la linea A-B
2. Interrogare API Open-Meteo Elevation: `https://api.open-meteo.com/v1/elevation?latitude=X&longitude=Y`
3. Calcolare curvatura terrestre: `earthBulge = distance^2 / (2 * R * k)` con k-factor 4/3 (rifrazione atmosferica)
4. Calcolare zona di Fresnel: `fresnelRadius = sqrt(n * lambda * d1 * d2 / (d1 + d2))` con lambda dalla frequenza LoRa
5. Disegnare profilo: terreno, linea di vista, zona di Fresnel. Verde = sgombro, rosso = ostruito

### UI
- Selezione punti: click su mappa o scelta da contatti con GPS
- Parametri configurabili: altezza antenna (0-122 m), k-factor
- Grafico: BView custom con DrawLine/FillRect/DrawString
- Risultato: distanza, azimut, clearance %, raccomandazione (OK/ostruito)

### Motivazione
Feature unica e differenziante. Nessun altro client desktop la offre. Valore enorme per chi piazza repeater in montagna: verificare il link dal divano prima di salire con l'attrezzatura.

**Difficolta**: Alta
**File coinvolti**: nuovo LoSWindow.cpp/h, nuovo ElevationService.cpp/h, integrazione con MapView per selezione punti

### Note tecniche Haiku
- HTTP API: `BUrlRequest` (Network Kit) o socket POSIX con TLS
- JSON parse: riutilizzare parser minimale (stile ProfileWindow)
- Rendering grafico: `BView::Draw()` custom
- Calcoli geodetici: formula di Haversine per distanze, bearing

---

## Priorita suggerita

| # | Feature | Difficolta | Stato |
|---|---------|-----------|-------|
| 1 | Contact Groups + Channel Muting | Bassa | COMPLETATO |
| 2 | GIF Animate via GIPHY | Media | COMPLETATO |
| 3 | Image Sharing via LoRa | Media | COMPLETATO |
| 4 | SAR Markers | Bassa | COMPLETATO |
| 5 | Emoji Rendering | Bassa | COMPLETATO |
| 6 | OSM Map Tiles | Media | COMPLETATO |
| 7 | UI Settings Persistence | Bassa | COMPLETATO |
| 8 | SMAZ Compression | Media | Da fare |
| 9 | Message Retry | Media-Alta | Da fare |
| 10 | Offline Map Tiles (bulk download) | Media | Da fare |
| 11 | Line-of-Sight Analysis | Alta | Da fare |

---

## Riferimenti
- meshcore-open: https://github.com/zjs81/meshcore-open
- SMAZ algorithm: https://github.com/antirez/smaz
- Open-Meteo Elevation API: https://open-meteo.com/en/docs/elevation-api
- CayenneLPP format: https://docs.mydevices.com/docs/lorawan/cayenne-lpp
- OSM tile server: https://tile.openstreetmap.org/{z}/{x}/{y}.png
