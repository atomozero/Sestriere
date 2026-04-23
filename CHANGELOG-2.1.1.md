# Sestriere 2.1.1 — Bug-fix release

Internal beta for testing. No new features; only fixes to issues
reported during 2.1.0 use.

## Fixes

### Messaging

- **Channel messages arriving corrupted on stock clients.**
  Sestriere SMAZ-compressed every channel message with an `s:` prefix.
  Stock MeshCore clients (phone app, etc.) don't understand the prefix
  and rendered the compressed bytes as garbage. Channels now always go
  out as plaintext; DMs still compress between Sestriere peers.

- **Room and repeater messages appearing as garbled symbols on other
  devices ([#13](https://github.com/atomozero/Sestriere/issues/13),
  [#14](https://github.com/atomozero/Sestriere/issues/14)).** Same
  SMAZ issue as channels: messages sent to rooms (type 3) and
  repeaters (type 2) are forwarded to all connected clients, most of
  which don't speak SMAZ. Compression is now skipped for room and
  repeater contacts. SMAZ remains active only for peer-to-peer DMs
  where both sides are Sestriere.

- **Incoming plaintext messages starting with `s:` losing their first
  two characters.** When SMAZ decompression failed, the receive path
  stripped the prefix on the assumption it was a stale marker — so a
  user typing `s:ok` from a stock client saw `ok`. The fallback now
  leaves the text untouched.

- **Delivery status written to the wrong chat row after reconnect.**
  Pending outgoing messages preserved across a disconnect kept the
  `chatViewIndex` they had before the chat view was cleared. Once the
  view was rebuilt with fresh indices, `RSP_SENT` updates landed on a
  different message (or past the end of the list). Transient per-
  attempt state (`expectedAck`, grace-period flags) is now also reset.

### Repeater / Room login ([#8](https://github.com/atomozero/Sestriere/issues/8), [#9](https://github.com/atomozero/Sestriere/issues/9))

- **Contacts disappearing after repeater/room login.** After a
  successful login, Sestriere triggered an incremental contact sync
  (`since` timestamp from the previous sync). The sync handler moved
  all existing contacts to a temporary list, but if the device
  returned zero updated contacts (nothing changed since last sync),
  the temporary list was discarded — leaving the sidebar empty.
  Fixed: force a full sync (`since=0`) after every login, and
  preserve unmatched contacts from the temporary list during
  incremental syncs instead of deleting them.

- **Logins intermittently rejected with the correct password.** Two
  independent bugs combined to make this unreliable:
  1. Copy-pasted passwords carrying a trailing space or newline were
     sent as-is; the hash on the repeater side did not match.
  2. The password field had no input cap, so typing 20 chars gave no
     feedback that only the first 15 would actually be used.
  Fixed: trim whitespace on submit, `SetMaxBytes(15)` on the text
  control. The wire-layer cap remains 15 chars, matching MeshCore's
  `char password[16]` buffer (15 usable + null terminator).

- **Logins failing when the companion device's RTC drifted from the
  room server's clock.** MeshCore stamps the login packet with the
  companion's local time, and rooms/repeaters reject timestamps that
  fall outside their anti-replay window. Sestriere never synced the
  RTC, so any device with a drifted or reset clock could not log in.
  We now sync the device RTC to host time right after the handshake
  completes, and again just before each login attempt as a safety
  net.

### Voice recording ([#6](https://github.com/atomozero/Sestriere/issues/6))

- **Microphone recording always failing with "Could not start
  recording".** `BMediaRecorder::Connect(format)` (single-argument
  form) connects to the **audio mixer**, not the audio input node.
  The mixer's native output (48kHz stereo float32) is incompatible
  with the requested 8kHz mono int16, so the connection was refused.
  Additionally, the recording callback assumed incoming data was
  always int16, which would corrupt audio from float32 hardware.
  Fixed: connect to the actual audio input node via
  `BMediaRoster::GetAudioInput()` (matching SoundRecorder's
  approach), accept the hardware's native format with wildcard, and
  convert any sample format (float32/int32/int16/uint8/int8) to
  8kHz mono int16 in the recording callback.

### Network map ([#12](https://github.com/atomozero/Sestriere/issues/12))

- **SNR values on map links showing 4x actual (e.g. 53 dB instead
  of 13 dB).** Trace route SNR bytes are stored in Q6.2 fixed-point
  format (value × 4) in the protocol, but `HandleTraceData()` was
  not dividing by 4 — unlike every other SNR parser in the app.

### Telemetry ([#11](https://github.com/atomozero/Sestriere/issues/11))

- **Sensor telemetry window not scrolling past 3-4 devices.** The
  content view was resized to `max(contentHeight, visibleHeight)`
  instead of always using the full content height, and the
  `BScrollView` wasn't getting the explicit preferred size it needs
  to compute scroll range on Haiku R1 beta5.

### USB serial connection ([#10](https://github.com/atomozero/Sestriere/issues/10))

- **USB device sometimes not recognized on connect.** Three issues
  combined: (1) port enumeration listed "ghost" entries that
  persisted in `/dev/ports/` after a USB unplug — these appeared in
  the menu but failed to open; (2) the 100 ms DTR/RTS stabilization
  delay was too short for some USB-to-UART bridges (CP210x, CH340);
  (3) auto-connect was a single-shot timer — if the device wasn't
  ready at that exact moment, no retry occurred. Fixed: validate
  each port with `open()`/`close()` before listing, increase delay
  to 250 ms, and retry auto-connect up to 3 times with a port
  re-scan on each attempt.

### Device info dialog

- **"Frequency: 869618.000 MHz".** `fRadioFreq` is stored in Hz; the
  Device Info alert divided by 1000 instead of 1e6, labelling kHz as
  MHz. Now shows "869.618 MHz" as expected.

## Upgrade notes

Drop the new `Sestriere` binary over the old one. No database,
settings, or on-disk layout changes. Existing contacts, channels,
messages, and SMAZ-compressed DM history all continue to work.
