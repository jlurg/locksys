# LockSys CGW simulator

| Field | Value |
|---|---|
| Package | `locksys-cgw-sim` (uv workspace member) |
| Contract | LS-SAIC-001 section 8 (APP ↔ CGW protocol 1.0) |
| Owner | jlurg |

## Purpose

WebSocket server that implements the CGW side of the APP protocol 1.0 for APP development and host integration tests: handshake with mutual authentication, tagged frames, session policies, window and door command handling and status pushes, backed by a DCU plant model. It shares the session cryptography and the generated protobuf stubs with the HIL APP simulator (`locksys_hil.protocols.appsim`), and both are checked against `interfaces/vectors/app_session_v1.json`.

## Usage

```sh
uv run locksys-cgw-sim --host 127.0.0.1 --port 8765            # random K_pair
uv run locksys-cgw-sim --k-pair 000102...1f --seed 7 --verbose  # fixed key
```

The simulator prints its pairing payload in the simulator form `locksys://pair?...&h=<host>:<port>`. The key is a simulator key, not a product secret. APP configurations: `app/config/sim_android_emulator.json` (host 10.0.2.2) and `app/config/sim_ios_simulator.json` (host 127.0.0.1), port 8765.

## Behaviour

| Area | Implemented |
|---|---|
| Transport | `ws://<host>:<port>/ws/v1`; subprotocol `locksys.v1` compared exactly (mismatch: close 4001); binary frames only (text: 1003); messages above 256 bytes: 1009; other paths: HTTP 404 |
| Handshake | ServerHello, ClientAuth proof check (constant time), K_sess by HKDF-SHA256, AuthResult(OK) with server proof and session parameters, Ping right after AuthResult; REJECTED_AUTH (4002), REJECTED_VERSION (4001), handshake time-out (4006) |
| Session policies | Single controller: same `client_id` pre-empts (SessionClose, 4004), another `client_id` gets REJECTED_BUSY (4003) while the controller is alive; failure throttling (REJECTED_RATE_LIMIT, 4006); rate limit and integrity violations (1008); session time-out (SessionClose, 4004) |
| Window | Press admission (DCU alive and mode, RTT gate, `press_id`, direction, `hold_ms`); keep-alive supervision `t_cgw_ka_to_ms`; CGW backstop; direction change and link-quality latches with the second CommandAck and Notice |
| Door | CommandAck, lock pulse of `t_lock_pulse_ms`, DoorCommandResult; repeated `request_id` re-acknowledged in flight and answered from the result cache when completed |
| Status | StatusUpdate on change, on StatusRequest and periodically (`t_status_push_idle_ms`, `t_status_push_motion_ms`) |
| Plant | Lock state from a simulated position switch, free-spinning window motor (stage A), temperature random walk (seeded) |

Not yet implemented (LATER): the fault-injection control port 8766 (dropped or delayed Pong, stale status, DCU offline, forced close codes, oversize frames, DCU-initiated stops), the pairing window and a timestamped frame trace.

## Design

`core.CgwSimCore` is a sans-I/O engine: every call takes the current time in milliseconds and returns the frames and close codes per connection, so all session rules are unit-tested deterministically (`tests/test_cgw_sim_session.py`). `server.SimServer` binds it to a websockets 17 server and ticks it every `t_cgw_tick_ms`.
