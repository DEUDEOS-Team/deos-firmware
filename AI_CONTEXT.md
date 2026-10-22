# DEOS Firmware AI Context

```yaml
context_schema: 1
purpose: canonical-first-pass-context-for-code-agents
repository: DEUDEOS-Team/deos-firmware
snapshot:
  firmware: 670da617c1cd35b671ae5430a7cb7b1729160fa0
  docs: 1c055d51d896203906c6e77dfd794324eefba348
  deos_core: 617d7ffe6dca3ac7fd8c804b9240b21b11cbaad8
  deos_protocol: 70d846f0320d189f8a35242490a2ca1f841c6446
  deos_transport_can: 8199e1a7b2bcef8b584edcca7df0fe51bcc7f1da
confidence_rule: source-code-overrides-this-file
```

## AGENT_BOOT_SEQUENCE

1. Read `AGENTS.md`.
2. Read this file completely before broad repository searches.
3. Select the task domain from `TARGETED_READ_MAP`; read only the listed source files plus files directly affected by the task.
4. If this file conflicts with code, code is authoritative. Correct this file in the same change.
5. Never infer wire values, payload layouts, routing, or node ownership from memory. Use the canonical source paths below.
6. A task is incomplete when it changes a fact represented here without updating this file.

Do not rescan the entire repository unless this file is missing, internally inconsistent, or the task explicitly requires a repository-wide audit.

## AUTHORITY_AND_BOUNDARIES

Priority, highest first:

1. `AGENTS.md`: permissions and mandatory workflow.
2. Compile-time definitions and implementations in the checked-out source.
3. This file: fast operational model and lookup index.
4. `docs/DEOS_ARCHITECTURE_KNOWLEDGE.md`: routing/LoRa history and design intent.
5. Other READMEs, reports, PDFs, and archived ICD material: explanatory; may lag code.

Mutation boundary:

- Agents may modify application code under `apps/` and repository-level support files when the task requires it.
- Everything under `lib/` is a Git submodule and read-only to agents without Emir Arkalı's explicit approval. This includes `deos_core`, `deos_protocol`, and `deos_transport_can`.
- `docs/` is also a Git submodule. Prefer updating this root context for cross-repository operational facts. Modify the docs submodule only when explicitly requested or authorized.
- A required core/library change must follow the issue workflow in `AGENTS.md`; do not patch the submodule directly.

## REPOSITORY_MODEL

```text
apps/                       independently built Zephyr applications
  main_node/                central router/gateway, state authority, lighting, IMU, LoRa, Ethernet, CAN
  traction/                 CAN-FD traction endpoint, current active endpoint example
  bms_gateway/              CAN-FD hosted-node gateway for BMS_MAIN and BMS_AUX
  steering/                 skeleton/incomplete application
  brake/                    placeholder directory; no buildable application at snapshot
  dummy_can/                CAN diagnostic/test node
  dummy_uart/               UART/frame test application
  lora_transmitter/         standalone LoRa transport test application
  simple_uart_listener/     standalone UART listener
lib/                        read-only submodules
  deos_core/                Zephyr runtime: dispatch, router, subscriptions, faults, node contexts
  deos_protocol/            portable C99 message/frame/codec/registry layer
  deos_transport_can/       Zephyr asynchronous CAN-FD transport and 29-bit CAN codec
tools/lora_test_suite/      GROUND_CONTROL-side Python protocol/test tools
tools/lora_config/          Ebyte radio configuration only
docs/                       separate documentation submodule
tests/desktop/              host-side tests/build artifacts
west.yml                    Zephyr revision/workspace manifest
```

Applications currently compile library `.c` files explicitly in their `CMakeLists.txt`; do not assume automatic module linkage. Preserve system includes such as `<deos/deos.h>` and `<deos/deos_can.h>`.

## PROTOCOL_CONSTANTS

Canonical lookup paths:

- IDs/enums/version/timing: `lib/deos_core/include/deos/deos_icd.h`
- message and payload structs: `lib/deos_core/include/deos/deos_types.h`
- public runtime API: `lib/deos_core/include/deos/deos.h`
- universal frame: `lib/deos_core/include/deos/deos_frame.h`
- fault IDs/API: `lib/deos_core/include/deos/deos_fault.h`
- CAN API: `lib/deos_transport_can/include/deos/deos_can.h`

Stable snapshot facts:

```yaml
protocol_version: 0x11
max_payload_bytes: 60
universal_frame:
  header_bytes: 9
  layout: [destination, source, priority, version, class, service, command, sequence, payload_length, payload]
can_fd:
  arbitration_bitrate: 500000
  data_bitrate: 2000000
  id_bits:
    priority: 28..26
    class: 25..22
    service: 21..16
    destination: 15..8
    source: 7..0
```

Never serialize C enums or unpacked structs by assuming host ABI size. Wire payloads must use the packed ICD payload structs or explicit fixed-width bytes/little-endian codec helpers. State payloads are one byte.

### NODE_REGISTRY

| ID | Symbol | Runtime ownership / route |
|---:|---|---|
| `0x01` | `DEOS_NODE_TEXTUAL` | Reserved identity; do not confuse with DIAG_TOOL |
| `0x02` | `DEOS_NODE_GROUND_CONTROL` | Python/ground station over UART LoRa |
| `0x03` | `DEOS_NODE_MAIN_STM32` | `apps/main_node`; local on router |
| `0x04` | `DEOS_NODE_ROS_BRIDGE` | Ethernet ROS bridge |
| `0x10` | `DEOS_NODE_TRACTION` | `apps/traction`; CAN-FD |
| `0x11` | `DEOS_NODE_STEERING` | intended `apps/steering`; CAN-FD |
| `0x12` | `DEOS_NODE_BRAKE` | intended `apps/brake`; CAN-FD |
| `0x13` | `DEOS_NODE_IMU` | hosted locally by `main_node` |
| `0x20` | `DEOS_NODE_BMS_MAIN` | hosted by `bms_gateway`; CAN-FD |
| `0x21` | `DEOS_NODE_BMS_AUX` | hosted by `bms_gateway`; CAN-FD |
| `0x30` | `DEOS_NODE_BMS_GATEWAY` | physical `apps/bms_gateway`; CAN-FD |
| `0xF0` | `DEOS_NODE_DIAG_TOOL` | Ethernet textual route; `dummy_can` also uses it for testing |
| `0xFE` | `DEOS_NODE_CAN_BROADCAST` | local delivery plus CAN-only forwarding |
| `0xFF` | `DEOS_NODE_GLOBAL_BROADCAST` | local delivery plus all registered physical transports except ingress |

### SERVICES_AND_COMMANDS

```yaml
SYSTEM_0x00:
  GET_STATE: 0x01
  SET_STATE: 0x02
  GET_MODE: 0x03
  SET_MODE: 0x04
  RESET_NODE: 0x05
  GET_NODE_INFO: 0x06
  HEARTBEAT: 0x07
  PING: 0x08
  PONG: 0x09
  SUBSCRIBE: 0x0A
  UNSUBSCRIBE: 0x0B
  GET_SUBSCRIPTIONS: 0x0C
TRACTION_0x01: {SET_TARGET_SPEED: 0x01, GET_STATUS: 0x02, SET_GEAR: 0x04, SET_TORQUE_LIMIT: 0x05}
STEERING_0x02: {SET_TARGET_ANGLE: 0x01, GET_STATUS: 0x02, SET_MODE: 0x04}
BRAKE_0x03: {SET_TARGET: 0x01, GET_STATUS: 0x02, SET_MODE: 0x04, SET_PARK: 0x05, SET_PREFILL: 0x06}
BMS_0x04: {GET_STATUS: 0x01, STATUS: 0x02, GET_CELL_VOLTAGES: 0x03, CELL_VOLTAGE: 0x04, GET_TEMPERATURES: 0x05, TEMPERATURE: 0x06, GET_BALANCING: 0x07, BALANCING: 0x08}
CALIBRATION_0x05: {START: 0x01, ABORT: 0x02, GET_STATUS: 0x03, SAVE_RESULT: 0x04, CLEAR_RESULT: 0x05}
DIAGNOSTIC_0x06: {GET_HEALTH: 0x01, GET_FAULTS: 0x02, CLEAR_FAULTS: 0x03}
SENSORS_0x07: {IMU_DATA: 0x01}
LIGHTING_0x08: {TURN_ON: 0x01, TURN_OFF: 0x02, GET_STATUS: 0x03, STATUS: 0x04}
PARAMETER_COMMON: {GET: 0xE0, SET: 0xE1, SAVE: 0xE2, RESTORE_DEFAULTS: 0xE3, GET_INFO: 0xE4}
```

Message classes: `SAFETY=0`, `COMMAND=1`, `STATUS=2`, `EVENT=3`, `REQUEST=4`, `RESPONSE=5`, `CONFIG=6`, `NETWORK=7`. State values: `INIT=0`, `STANDBY=1`, `READY=2`, `ACTIVE=3`, `CALIBRATING=4`, `SAFE=5`, `FAULT=6`, `UNKNOWN=0xFF`.

## CORE_RUNTIME_MODEL

Initialization order for an endpoint:

```text
deos_init(config)
-> optional deos_register_local_node(...) before start
-> deos_set_node_info[_for_node](...)
-> deos_register_handler[_for_node](...)
-> transport init (CAN: deos_can_transport_init)
-> deos_start()
```

Rules:

- `deos_init` registers the primary local node and initializes faults, dispatch, router, and subscriptions.
- Hosted nodes require `hosted_nodes_enabled=true`; register them before `deos_start`.
- `deos_send` sends as the primary node. `deos_send_from_node` requires the source to be a registered local node.
- `deos_send_response` addresses the original source and prepends a one-byte `deos_result_t` to optional response data.
- Handler identity is `(local node, message class, service, command)`. Match the ICD class, not only service/command.
- Transport RX decodes and calls `deos_feed_message` or `deos_feed_frame` with the actual ingress transport.
- Transport TX callbacks registered with the router must be nonblocking. Queue with `K_NO_WAIT`; physical I/O belongs in a dedicated worker thread. Queue-full is a drop plus fault, never a caller stall.
- Core uses static allocation. Do not introduce heap allocation in real-time paths.

## ROUTING_MODEL

`main_node` is the only current router. Its policy in `apps/main_node/src/MainApp.cpp` maps:

```yaml
MAIN_STM32: LOCAL
GLOBAL_BROADCAST: BROADCAST_ALL
CAN_BROADCAST: CAN_FD
GROUND_CONTROL: UART_LORA
ROS_BRIDGE: ETH_ROS_BRIDGE
DIAG_TOOL: ETH_TEXTUAL
TRACTION..BMS_GATEWAY: CAN_FD
otherwise: UNKNOWN/drop
```

Invariant: Split Horizon. A received packet is not forwarded back to its ingress transport. Broadcasts are delivered to local handlers, then forwarded to registered physical transports except ingress. `LOCAL` is a pseudo-transport used for messages created by `deos_send`; it is not hardware. A locally created global broadcast therefore reaches every registered physical transport.

Broadcast intent:

- `GLOBAL_BROADCAST (0xFF)`: low-frequency critical system-wide events such as state/SAFE transitions.
- `CAN_BROADCAST (0xFE)`: CAN-only frequent traffic such as heartbeat/status, avoiding the 2.4 kbps LoRa air link.

## TRANSPORT_MODEL

### CAN_FD

- Implementation: `lib/deos_transport_can/src/deos_can_transport.c` and `deos_can_codec.c`.
- RX ISR only queues `struct can_frame`; RX worker decodes and feeds core.
- TX API queues `deos_message_t` with `K_NO_WAIT`; TX worker performs `can_send` with a bounded timeout.
- Router installs promiscuous extended-ID filter. Endpoint installs filters for local node(s) and broadcasts; verify implementation when adding hosted nodes.
- `deos_can_transport_init` registers the router callback and currently calls `deos_can_transport_start` itself. Some apps call `deos_can_transport_start` again; this is idempotent and returns success when already started.

### UART_LORA

- Application-owned transport: `apps/main_node/src/transport/uart_lora.cpp`.
- STM32 UART: 115200 baud. Radio air rate: 2.4 kbps.
- RX IRQ writes a ring buffer and gives a semaphore; worker thread parses immediately. Do not add polling sleeps.
- TX callback queues to a dedicated TX thread with `K_NO_WAIT`.
- Outer radio framing uses magic `AA 55`, flags, length, universal DEOS frame, CRC16-CCITT. Inspect the implementation and Python tools before changing framing.
- `tools/lora_config/` configures Ebyte hardware; it is not the protocol test suite.

### ETHERNET

- Textual TCP: `apps/main_node/src/transport/eth_textual.cpp`, routed to `DIAG_TOOL`.
- ROS UDP: `apps/main_node/src/transport/eth_ros_bridge.cpp`, routed to `ROS_BRIDGE`.
- Both use dedicated TX queues/threads. Preserve nonblocking router callbacks.

## APPLICATION_SNAPSHOTS

### main_node

```yaml
primary_node: MAIN_STM32_0x03
router: true
hosted_nodes: [IMU_0x13]
transports: [CAN_FD, UART_LORA, ETH_TEXTUAL, ETH_ROS_BRIDGE]
state_storage: Zephyr atomic_t
state_authority: MainApp.set_state
critical_watchdog_nodes: [BMS_MAIN, BMS_AUX, TRACTION, BRAKE, STEERING]
```

Registered application handlers include heartbeat, `SET_STATE` as COMMAND and REQUEST, `GET_STATE` as REQUEST, and lighting commands/status request. Core handles some system behavior automatically; inspect dispatch before duplicating PING/PONG or subscription logic. `MainApp::set_state` applies local LED safety behavior before broadcasting so transport trouble cannot delay local SAFE indication. LED shared state is mutex-protected.

### traction

```yaml
node: TRACTION_0x10
router: false
transport: CAN_FD
board_reference: nucleo_h563zi
handlers: [SYSTEM_PING, SYSTEM_HEARTBEAT, SYSTEM_SET_STATE, SYSTEM_GET_STATE, TRACTION_SET_TARGET_SPEED, TRACTION_SET_GEAR, TRACTION_SET_TORQUE_LIMIT, TRACTION_GET_STATUS, LIGHTING_STATUS]
```

`apps/traction/src/main.c` is the best current C endpoint template. It models state, target speed/acceleration, gear, torque limit, simulated telemetry, command validation, responses, CAN startup, and state LEDs.

### bms_gateway

```yaml
primary_node: BMS_GATEWAY_0x30
hosted_nodes: [BMS_MAIN_0x20, BMS_AUX_0x21]
router: false
transport: CAN_FD
external_bus: Daly BMS protocol
language: C++17
```

Uses node-specific handlers and `deos_send_from_node` to represent each hosted BMS. Preserve independent requester/state tracking per BMS.

### incomplete_or_test_apps

- `steering`: build skeleton; do not treat as a complete DEOS endpoint template.
- `brake`: placeholder at snapshot.
- `dummy_can`: diagnostic integration/test behavior, not production node architecture.
- UART/LoRa sample apps: physical/framing experiments, not replacements for `main_node` transport.

## NEW_NODE_RECIPE

For a new CAN endpoint under `apps/<node>`:

1. Select an existing node/service/command ID. If absent, a library/ICD change is required; stop and follow the core issue/approval rule.
2. Copy structural patterns from `apps/traction`, not from dummy apps.
3. Create `CMakeLists.txt`, `prj.conf`, `app.overlay`, board overlay, and `src/main.c` or C++ sources.
4. Include `<deos/deos.h>`, `<deos/deos_can.h>`, and Zephyr logging. Never `printf`.
5. Configure `deos_config` with the exact node ID; endpoint uses `router_enabled=false`.
6. Initialize local hardware and core; register handlers before `deos_start`.
7. Validate every incoming payload length before reading. Use fixed-width packed payload types or explicit codec helpers.
8. Initialize CAN transport, handle every return value, then start core.
9. Keep callbacks short and nonblocking. Move physical I/O or long work to queues/workers.
10. Raise defined faults for operational failures; node-specific fault IDs begin at `0x0100`.
11. Build independently with explicit board and app path.
12. Update `APPLICATION_SNAPSHOTS`, `NODE_REGISTRY`, routing if applicable, and `snapshot` hashes in this file.

## CONCURRENCY_AND_SAFETY

- ISR work: capture/queue/signal only. No parsing, logging floods, sleeps, socket I/O, or blocking device calls in ISR.
- Router TX callback: enqueue only; use `K_NO_WAIT`.
- Mutable data shared by main/RX/TX threads must use Zephyr atomics, mutexes, message queues, or ownership transfer.
- Apply local safety effects before network publication. A failed transport must not prevent SAFE state, braking, or local fault indication.
- Avoid lock acquisition followed by `deos_send` when local broadcast dispatch can reenter the same handler and lock.
- Bound all I/O waits. Queue saturation must be observable through logs/faults.
- LoRa bandwidth is scarce; never route periodic CAN telemetry globally without explicit design intent.

## BUILD_AND_VALIDATION

Never run an ambiguous root build. Use:

```bash
west build -b <board> apps/<app> -d build/<app> --pristine
west flash -d build/<app>
```

Known board overlays include `nucleo_h563zi` and, for several apps, `nucleo_h7a3zi_q`. Use the board actually represented by the target app's `boards/<board>.overlay`.

Project helper equivalents when run from the intended app directory:

```bash
zephyrproject -clean
zephyrproject -clean-flash
zephyrproject -flash-only
```

Validation minimum:

- `git diff --check`
- explicit app build for every changed buildable application
- protocol/routing/LoRa changes: targeted Python tool or hardware test when available
- no unrelated submodule pointer changes

## TARGETED_READ_MAP

| Task | Read after this file |
|---|---|
| New endpoint/node | `apps/traction/src/main.c`, its CMake/config/overlays, public `deos.h`, `deos_types.h`, exact ICD enum/payload |
| Main router/state | `apps/main_node/src/MainApp.cpp/.hpp`, `NetworkMonitor.*`, `deos_router.c`, `deos_dispatch.c` |
| CAN behavior | `deos_can.h`, `deos_can_transport.c`, `deos_can_codec.c`, target app overlay |
| LoRa/UART | `docs/DEOS_ARCHITECTURE_KNOWLEDGE.md`, `uart_lora.cpp/.hpp`, matching Python test/framing code |
| Ethernet | corresponding `eth_textual.*` or `eth_ros_bridge.*`, routing policy, network config |
| BMS | `apps/bms_gateway/src/*`, BMS payload structs/commands, relevant ICD document only if code is ambiguous |
| Faults | `deos_fault.h`, public fault API, call sites in target app |
| Pub/sub/heartbeat | public subscription APIs, `deos_subscription.c`, `deos_network.c`, target handlers |
| Wire format | `deos_types.h`, `deos_icd.h`, `deos_frame.h/.c`, transport codec; compare portable `deos_protocol` mirror |
| Build/board | app `CMakeLists.txt`, `prj.conf`, `app.overlay`, exact board overlay, `west.yml` |

## CONTEXT_MAINTENANCE_PROTOCOL

Update this file in the same commit when any represented fact changes.

| Code change | Mandatory context update |
|---|---|
| Node ID/owner/hosted-node change | `NODE_REGISTRY`, `APPLICATION_SNAPSHOTS`, snapshot hashes |
| Service/command/state/class value | `SERVICES_AND_COMMANDS` or constants, snapshot hashes |
| Payload/frame/CAN layout | `PROTOCOL_CONSTANTS`, canonical paths if moved, snapshot hashes |
| Route or broadcast behavior | `ROUTING_MODEL`, application snapshot, targeted read map if needed |
| Transport framing/threading/queue behavior | relevant `TRANSPORT_MODEL` and concurrency rule |
| App added/removed/completed | `REPOSITORY_MODEL`, `APPLICATION_SNAPSHOTS`, `NEW_NODE_RECIPE` if pattern changes |
| Public core API/lifecycle | `CORE_RUNTIME_MODEL`, recipe, targeted read map |
| Build board/tooling/config | `BUILD_AND_VALIDATION`, app snapshot |
| Safety/concurrency invariant | `CONCURRENCY_AND_SAFETY` and affected snapshot |
| Submodule pointer update | matching `snapshot` commit and every affected fact |

Maintenance procedure:

1. Edit facts, not history. Remove obsolete statements instead of appending a changelog.
2. Keep identifiers, numeric values, paths, and ownership explicit.
3. Refresh `snapshot` with `git rev-parse HEAD` and `git -C <submodule> rev-parse HEAD`. For a commit that edits this file, `firmware` may temporarily name the verified pre-change base; update it on the next context-affecting change rather than inventing a future hash.
4. Run targeted consistency searches for the changed identifiers.
5. If uncertainty remains, mark it `UNKNOWN` with the exact source needing verification; do not guess.

## KNOWN_LIMITS_AT_SNAPSHOT

- Several applications duplicate explicit library source lists in CMake; adding a core source may require synchronized app CMake updates.
- `steering` and `brake` are not production-complete endpoint references.
- Documentation and portable protocol headers can lag the runtime submodule. Verify duplicate ICD/type definitions before cross-platform wire changes.
- Router dispatch invokes registered TX callbacks sequentially; correctness depends on the nonblocking queue-only callback contract.
- Hardware behavior cannot be proven by compilation alone; CAN, LoRa, Ethernet, GPIO, and timing changes require target validation.
