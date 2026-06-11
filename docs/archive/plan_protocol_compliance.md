# Piano: Conformità 100% MeshCore Companion Radio Protocol V3

Riferimento: [Companion Radio Protocol Wiki](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol)

---

## Riepilogo problemi trovati

| # | Problema | Gravità | File coinvolti |
|---|---------|---------|----------------|
| 1 | SNR ×4 non diviso nei V3 DM/Channel | **BUG critico** | MainWindow.cpp |
| 2 | RSP_TUNING_PARAMS (23) non gestito | **Mancante** | Constants.h, MainWindow.cpp |
| 3 | CMD_GET_ALLOWED_REPEAT_FREQ (60) mancante | **Mancante** | Constants.h, ProtocolHandler |
| 4 | RESP_ALLOWED_REPEAT_FREQ (26) non gestito | **Mancante** | Constants.h, MainWindow.cpp |
| 5 | Costanti inutilizzate/duplicate negli offset V3 | Cleanup | Constants.h |

---

## Fix 1: BUG SNR ×4 nei messaggi V3

### Problema

Il firmware invia il campo SNR nei messaggi V3 (RESP_CODE_CONTACT_MSG_RECV_V3 = 0x10,
RESP_CODE_CHANNEL_MSG_RECV_V3 = 0x11) come **SNR × 4** (formato Q6.2 fixed-point in int8).
Tutti gli altri handler dividono correttamente per 4, ma `_HandleContactMsgRecv` e
`_HandleChannelMsgRecv` usano il valore raw.

### Confronto attuale

| Handler | Codice | Divisione |
|---------|--------|-----------|
| PUSH_RAW_DATA (0x84) | `snr / 4.0f` | ✓ Corretto |
| PUSH_CONTROL_DATA (0x8E) | `(int8)data[1] / 4.0f` | ✓ Corretto |
| PUSH_ADVERT (0x80) | `snr / 4` | ✓ Corretto |
| Status Response | `snrRaw / 4` | ✓ Corretto |
| **V3 DM (0x10)** | `snr = (int8)data[1]` — **raw** | ✗ BUG |
| **V3 Channel (0x11)** | `snr = (int8)data[1]` — **raw** | ✗ BUG |

### Impatto

Il valore SNR raw ×4 viene propagato senza divisione a:
- `chatMsg.snr` → bolle chat mostrano es. "SNR -24" invece di "SNR -6"
- `DatabaseManager::InsertMessage()` → valore 4× troppo grande nel DB
- `DatabaseManager::InsertSNRDataPoint()` → grafico SNR sfalsato
- `fChatHeader->SetConnectionInfo()` → header chat errato
- `fNetworkMapWindow->UpdateLinkQuality()` → colorazione link errata
- `fMissionControlWindow->AddSNRDataPoint()` → trend SNR sfalsato
- Log e MQTT publish con valore errato

I threshold in MessageView (`>0` verde, `>=-10` giallo, else rosso) sono calibrati per dB
reali. Con il bug, un SNR reale di -3 dB arriva come -12 e viene mostrato **rosso** invece
di **giallo**.

### Modifica

**File:** `src/MainWindow.cpp`

In `_HandleContactMsgRecv` (riga ~5044):
```cpp
// PRIMA (bug):
snr = (int8)data[kV3DmSnrOffset];

// DOPO (fix):
snr = (int8)data[kV3DmSnrOffset] / 4;  // V3 SNR is stored ×4
```

In `_HandleChannelMsgRecv` (riga ~5304):
```cpp
// PRIMA (bug):
snr = (int8)data[kV3ChSnrOffset];

// DOPO (fix):
snr = (int8)data[kV3ChSnrOffset] / 4;  // V3 SNR is stored ×4
```

### Test: `test_snr_scaling.cpp`

Verifiche:
1. Costanti offset V3 DM/Channel definite (kV3DmSnrOffset, kV3ChSnrOffset)
2. `_HandleContactMsgRecv` contiene `/ 4` dopo lettura SNR V3
3. `_HandleChannelMsgRecv` contiene `/ 4` dopo lettura SNR V3
4. PUSH_RAW_DATA handler contiene `/ 4.0f` (conferma invariato)
5. PUSH_CONTROL_DATA handler contiene `/ 4.0f` (conferma invariato)
6. PUSH_ADVERT handler contiene `snr / 4` (conferma invariato)
7. Status response handler contiene `/ 4` (conferma invariato)
8. Verifica che SNR in log DM sia formattato come `"SNR:%d"` (post-divisione)
9. Verifica che SNR in log Channel non mostri valori ×4

---

## Fix 2: RSP_TUNING_PARAMS (codice 23)

### Problema

`CMD_GET_TUNING_PARAMS` (43) è definito e il metodo `SendGetTuningParams()` esiste in
ProtocolHandler, ma la risposta del firmware (codice 23) non è né definita né gestita.
Il frame arriva, entra nel `default:` di `_ParseFrame` e viene scartato con
"Unknown response: 0x17".

### Formato risposta (da wiki)

```
[0] = 23 (RESP_CODE_TUNING_PARAMS)
[1-4] = rxdelay_base (uint32 LE)
[5-8] = airtime_factor (uint32 LE)
[9-16] = reserved (8 bytes, all zeros)
```

Stesso formato del payload di `CMD_SET_TUNING_PARAMS`.

### Modifiche

**File:** `src/Constants.h`
```cpp
const uint8 RSP_TUNING_PARAMS = 23;
```

**File:** `src/MainWindow.h`
- Aggiungere dichiarazione: `void _HandleTuningParams(const uint8* data, size_t length);`

**File:** `src/MainWindow.cpp`

1. Aggiungere case in `_ParseFrame`:
```cpp
case RSP_TUNING_PARAMS:
    _HandleTuningParams(data, length);
    break;
```

2. Implementare `_HandleTuningParams`:
```cpp
void
MainWindow::_HandleTuningParams(const uint8* data, size_t length)
{
    if (length < 9) {
        _LogMessage("WARN", "TuningParams response too short");
        return;
    }

    uint32 rxDelayBase = ReadLE32(data + 1);
    uint32 airtimeFactor = ReadLE32(data + 5);

    _LogMessage("INFO", BString().SetToFormat(
        "Tuning params: rxDelay=%u, airtimeFactor=%u",
        rxDelayBase, airtimeFactor));

    // Aggiornare UI in SettingsWindow se aperto
    if (fSettingsWindow != NULL && fSettingsWindow->LockLooper()) {
        fSettingsWindow->SetTuningParams(rxDelayBase, airtimeFactor);
        fSettingsWindow->UnlockLooper();
    }
}
```

3. In SettingsWindow: verificare se `SetTuningParams()` esiste già o va aggiunto.

### Test: `test_tuning_response.cpp`

Verifiche:
1. `RSP_TUNING_PARAMS` definito in Constants.h con valore 23
2. `case RSP_TUNING_PARAMS:` presente in `_ParseFrame`
3. `_HandleTuningParams` esiste in MainWindow.cpp
4. Handler fa length check (`length < 9`)
5. Handler usa `ReadLE32` per rxDelayBase e airtimeFactor
6. Handler ha log con i due valori
7. `_HandleTuningParams` dichiarato in MainWindow.h

---

## Fix 3: CMD_GET_ALLOWED_REPEAT_FREQ (codice 60)

### Problema

Il firmware supporta `CMD_GET_ALLOWED_REPEAT_FREQ` (60) che restituisce le bande di
frequenza consentite per il repeating. Questo comando e la sua risposta
`RESP_ALLOWED_REPEAT_FREQ` (26) non sono implementati.

### Formato

**Comando:**
```
[0] = 60
```

**Risposta (codice 26):**
```
[0] = 26
[1-4] = lower_freq_1 (uint32 LE, kHz)
[5-8] = upper_freq_1 (uint32 LE, kHz)
[9-12] = lower_freq_2 (uint32 LE, kHz)
[13-16] = upper_freq_2 (uint32 LE, kHz)
... (array di coppie)
```

### Modifiche

**File:** `src/Constants.h`
```cpp
const uint8 CMD_GET_ALLOWED_REPEAT_FREQ = 60;
const uint8 RSP_ALLOWED_REPEAT_FREQ = 26;
```

**File:** `src/ProtocolHandler.h`
```cpp
void SendGetAllowedRepeatFreq();
```

**File:** `src/ProtocolHandler.cpp`
```cpp
void
ProtocolHandler::SendGetAllowedRepeatFreq()
{
    uint8 payload[1];
    payload[0] = CMD_GET_ALLOWED_REPEAT_FREQ;
    _SendFrame(payload, 1);
}
```

**File:** `src/MainWindow.h`
```cpp
void _HandleAllowedRepeatFreq(const uint8* data, size_t length);
```

**File:** `src/MainWindow.cpp`

1. Case in `_ParseFrame`:
```cpp
case RSP_ALLOWED_REPEAT_FREQ:
    _HandleAllowedRepeatFreq(data, length);
    break;
```

2. Handler:
```cpp
void
MainWindow::_HandleAllowedRepeatFreq(const uint8* data, size_t length)
{
    if (length < 9) {
        _LogMessage("INFO", "No allowed repeat frequencies reported");
        return;
    }

    size_t pairCount = (length - 1) / 8;
    BString info("Allowed repeat frequencies:");
    for (size_t i = 0; i < pairCount; i++) {
        uint32 lowerKHz = ReadLE32(data + 1 + i * 8);
        uint32 upperKHz = ReadLE32(data + 5 + i * 8);
        info.Append(BString().SetToFormat(
            " %.3f-%.3f MHz",
            lowerKHz / 1000.0, upperKHz / 1000.0));
    }
    _LogMessage("INFO", info);
}
```

### Test: `test_allowed_repeat_freq.cpp`

Verifiche:
1. `CMD_GET_ALLOWED_REPEAT_FREQ` definito con valore 60
2. `RSP_ALLOWED_REPEAT_FREQ` definito con valore 26
3. `SendGetAllowedRepeatFreq` metodo esiste in ProtocolHandler
4. `case RSP_ALLOWED_REPEAT_FREQ:` in `_ParseFrame`
5. `_HandleAllowedRepeatFreq` esiste in MainWindow.cpp
6. Handler fa length check
7. Handler usa `ReadLE32` per parsing frequenze

---

## Fix 4: Cleanup costanti V3 inutilizzate

### Problema

In `Constants.h` ci sono costanti V3 definite ma mai usate nel codice:
- `kV3DmRssiOffset = 2` — RSSI dal V3 DM, mai letto
- `kV3DmPathLenOffsetB = 3` — duplicato di offset, mai usato

### Modifica

Rimuovere le costanti inutilizzate o, se il firmware effettivamente invia RSSI al byte [2]
del V3 DM, leggere il valore e usarlo al posto di `fLastRssi`.

**Verifica da fare:** il firmware mette RSSI a data[2] nel V3 DM?
- Da wiki: byte [2-3] sono "reserved (0x00)" → **rimuovere le costanti inutilizzate**

**File:** `src/Constants.h`
```cpp
// Rimuovere:
// const size_t kV3DmRssiOffset = 2;    // reserved, non usato
// const size_t kV3DmPathLenOffsetB = 3; // duplicato, non usato
```

### Test

Nessun test dedicato — verificare che la build compili senza errori dopo la rimozione.

---

## Sequenza di implementazione

### Step 1: Fix SNR ×4
1. Modificare `MainWindow.cpp`: dividere per 4 in entrambi gli handler
2. Creare `test_snr_scaling.cpp`
3. Compilare test ed eseguire
4. Build completo: `make -j4`

### Step 2: RSP_TUNING_PARAMS
1. Aggiungere costante in `Constants.h`
2. Verificare se `SettingsWindow::SetTuningParams()` esiste
3. Aggiungere handler in `MainWindow.h/.cpp`
4. Aggiungere case in `_ParseFrame`
5. Creare `test_tuning_response.cpp`
6. Build completo

### Step 3: CMD/RSP Allowed Repeat Freq
1. Aggiungere costanti in `Constants.h`
2. Aggiungere metodo in `ProtocolHandler.h/.cpp`
3. Aggiungere handler in `MainWindow.h/.cpp`
4. Aggiungere case in `_ParseFrame`
5. Creare `test_allowed_repeat_freq.cpp`
6. Build completo

### Step 4: Cleanup costanti
1. Rimuovere costanti inutilizzate
2. Build completo per verificare nessun breakage

### Step 5: Aggiornare documentazione
1. Aggiornare `ROADMAP.md`: sezione conformità protocollo
2. Aggiornare tabella copertura CMD/RSP/PUSH

### Step 6: Test finali e commit
1. Eseguire tutti i test nuovi
2. Eseguire i test esistenti rilevanti (`test_protocol_commands`, `test_constants`, etc.)
3. Build completo finale
4. Commit unico senza riferimenti a Claude
