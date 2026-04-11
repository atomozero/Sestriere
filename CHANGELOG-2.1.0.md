# Sestriere 2.1.0 — Release Notes

**Data:** 2026-04-10
**Base:** 2.0.0 + 9 commit

## Protocollo: 100% MeshCore V3

Raggiunta piena compatibilita con la spec MeshCore Companion Radio Protocol V3
(era 90.4%, ora 94/94 codici).

Codici aggiunti:

- **CMD**: `CMD_IMPORT_CONTACT` (0x12)
- **RSP**: `RSP_PRIVATE_KEY` (0x0E), `RSP_DISABLED` (0x0F), `RSP_SIGN_START` (0x13), `RSP_SIGNATURE` (0x14)
- **PUSH**: `PUSH_PATH_DISCOVERY` (0x8D), `PUSH_CONTACT_DELETED` (0x8F), `PUSH_CONTACTS_FULL` (0x90)
- Rename `PUSH_RAW_RADIO_PACKET` -> `PUSH_LOG_RX_DATA` (allineamento naming spec)
- RSP_DEVICE_INFO: estrazione campi V9+ `client_repeat` e V10+ `path_hash_mode`

## Bug Fix (13 fix)

### Messaggi e delivery

1. **ACK matching con ackCode** — PUSH_SEND_CONFIRMED ora usa ackCode (bytes 1-4) per identificare il messaggio pending, non piu FIFO cieco
2. **Channel delivery status** — i messaggi canale mostrano DELIVERY_FAILED se SendChannelMsg fallisce, non piu checkmark fasullo
3. **Image/voice envelope session tracking** — `fImageEnvelopeWaiting` bool sostituito con `fImageEnvelopeSession` (uint32 session ID), fix invio concorrente
4. **UTF-8 truncation** — troncamento a 160 byte al confine di carattere UTF-8, non piu mid-sequence
5. **Queue full feedback** — messaggio DELIVERY_FAILED visibile in chat quando la coda pending e piena
6. **Room nick con ":"** — il parser usa l'ultimo ": " entro 32 char, non il primo (fix nomi con ":")
7. **SMAZ decompression fallback (DM + canale)** — su fallimento strip del prefisso "s:" e log warning, applicato sia a DM che canali
8. **DeleteMessage Y2038** — `sqlite3_bind_int` -> `sqlite3_bind_int64` per timestamp > 2^31

### Sessioni media e sicurezza

9. **Session cap a 32** — ImageSessionManager e VoiceSessionManager ora limitano a 32 sessioni concorrenti, evitando memory exhaustion da flood di envelope
10. **Sessioni complete purgate** — PurgeExpired rimuove anche le sessioni COMPLETE dopo TTL (era un memory leak progressivo)
11. **GIF canvas cap 4096x4096** — Rifiuta GIF malformati con dimensioni assurde per prevenire integer overflow nell'allocazione canvas
12. **MqttClient timer null-safety** — Puntatori timer azzerati dopo delete nel distruttore

### UI e debug

13. **DebugLogWindow pruning** — Aggiunto cap 256KB al log (cresceva senza limiti), fix thread safety su ShowWindow(), _CommandName ampliato da 23 a 89 codici con split TX/RX
14. **Network Map nodi ridimensionati** — Raggio base ridotto (max 14px, cap assoluto 22px dopo zoom), hub glow 2.5x -> 1.6x, pulse 1.5x -> 1.3x

## Test (66 pass, 0 fail)

Nuovi test aggiunti in questa release:

- `test_message_db` (26 test) — persistenza messaggi SQLite, dedup, channel keys, companion isolation, Y2038
- `test_message_flow` (19 test) — flusso DM/Room, parsing V2/V3, timestamp sanitization, self-echo, delivery, SMAZ
- `test_frame_security` (18 test) — audit sicurezza frame parser, bounds check su tutti i 34 handler, ReadLE32 protection

## Audit Sicurezza

### Frame parser (SerialHandler)
- Nessun buffer overflow: fFrameBuffer e 515B, length validata prima di scrivere
- Frame con length > 512 rifiutati immediatamente
- Connect() resetta sempre lo stato del parser

### Input validation (MainWindow, 34 handler)
- Tutti i ReadLE32/ReadLE16 hanno bounds check `length >=`
- Nessuna format string vulnerability
- Stringhe da frame sempre NUL-terminate con strnlen/strlcpy
- Contact frame (148B) completamente validato
- SMAZ decompression con buffer bounds e fallback

### Sessioni media
- Image/Voice session manager con cap 32 sessioni
- GIF decoder con canvas cap 4096x4096
- Fragment index validato contro totalFragments

## Dipendenze

```
pkgman install mosquitto_devel sqlite_devel curl_devel giflib_devel
```

Codec2: build da sorgente in `/boot/system/non-packaged`
