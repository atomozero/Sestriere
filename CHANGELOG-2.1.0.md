# Sestriere 2.1.0 — Release Notes (internal)

**Data:** 2026-04-09
**Base:** 2.0.0 + 4 commit

## Protocollo: 100% MeshCore V3

Raggiunta piena compatibilità con la spec MeshCore Companion Radio Protocol V3.
Codici aggiunti:

- **CMD**: `CMD_IMPORT_CONTACT` (0x12) — import card contatto da export
- **RSP**: `RSP_PRIVATE_KEY` (0x0E), `RSP_DISABLED` (0x0F), `RSP_SIGN_START` (0x13), `RSP_SIGNATURE` (0x14)
- **PUSH**: `PUSH_PATH_DISCOVERY` (0x8D), `PUSH_CONTACT_DELETED` (0x8F), `PUSH_CONTACTS_FULL` (0x90)
- Rename `PUSH_RAW_RADIO_PACKET` → `PUSH_LOG_RX_DATA` (allineamento naming spec)
- RSP_DEVICE_INFO: estrazione campi V9+ `client_repeat` e V10+ `path_hash_mode`

## Bug Fix (8 fix)

1. **ACK matching con ackCode** — PUSH_SEND_CONFIRMED ora usa ackCode (bytes 1-4) per identificare il messaggio pending, non più FIFO cieco
2. **Channel delivery status** — i messaggi canale mostrano DELIVERY_FAILED se SendChannelMsg fallisce, non più ✓ fasullo
3. **Image/voice envelope session tracking** — `fImageEnvelopeWaiting` bool sostituito con `fImageEnvelopeSession` (uint32 session ID), fix invio concorrente
4. **UTF-8 truncation** — troncamento a 160 byte al confine di carattere UTF-8, non più mid-sequence
5. **Queue full feedback** — messaggio DELIVERY_FAILED visibile in chat quando la coda pending è piena
6. **Room nick con ":"** — il parser usa l'ultimo `": "` entro 32 char, non il primo (fix nomi con ":")
7. **SMAZ decompression fallback** — su fallimento strip del prefisso "s:" e log warning
8. **DeleteMessage Y2038** — `sqlite3_bind_int` → `sqlite3_bind_int64` per timestamp > 2^31

## Test

83 test totali (66 pass, 7 skip per dipendenze non installate):

- `test_message_db` (26 test) — persistenza messaggi SQLite
- `test_message_flow` (19 test) — flusso DM/Room, parsing, delivery
- `test_frame_security` (18 test) — audit sicurezza frame parser e input validation

## Audit Sicurezza

Audit completo del frame parser (SerialHandler) e di tutti i 34 handler in MainWindow:

- Nessun buffer overflow: fFrameBuffer è 515B, length validata prima di scrivere
- Tutti i ReadLE32/ReadLE16 hanno bounds check
- Nessuna format string vulnerability
- Stringhe da frame sempre NUL-terminate
- Contact frame (148B) completamente validato

## Dipendenze

`pkgman install mosquitto_devel sqlite_devel curl_devel giflib_devel`
Codec2: build da sorgente in `/boot/system/non-packaged`
