# C serial port splitter: coding-agent handoff

Prepared and sources checked: September 22, 2026.

## 1. Assignment and scope

Implement a Windows 11 desktop command-line program in **C17** that lets **CCS 21 Serial Console and PuTTY receive the same TI Tiva LaunchPad UART output simultaneously**. Allow one selected terminal to transmit to the board. Preserve every byte within a successful, connected session and explicitly report conditions that invalidate delivery.

Build our own routing engine using Win32 serial APIs. Use an independently installed, validated virtual null-modem pair driver to provide the COM endpoints. The C executable is not a driver and cannot create functional serial devices just by assigning COM names.

This document gives implementation instructions. Defaults, limits, and test durations below are project decisions, not measured performance or manufacturer specifications. Do not claim that this architecture is fastest, lossless under arbitrary conditions, or already tested on the user's board.

Deliver a working implementation, automated tests, setup instructions, and measured results where equipment is available. If hardware or a usable virtual-port provider is missing, finish the independent code and mock tests; explicitly mark integration and hardware acceptance as pending.

**Version 1 excludes:** a custom kernel/UMDF driver, simultaneous command writers, dynamic writer switching, protocol parsing, automatic USB reconnection, a GUI, a Windows service, network transport, and transparent forwarding of every serial control operation. These may be later projects.

## 2. Accuracy audit of the earlier discussion

| Earlier point | Verified interpretation for this implementation |
| --- | --- |
| Windows will not share the ordinary physical COM handle | Confirmed by Microsoft's exclusive-open rules; this predates Windows 11. A second failing open is the likely explanation only if each terminal works independently. [Communications handles](https://learn.microsoft.com/en-us/windows/win32/devio/communications-resource-handles) |
| CCS debugging conflicts with PuTTY | Too broad. ICDI exposes debugger and COM interfaces. The conflict described here concerns two serial clients, not inherently JTAG debugging plus one serial client. [TM4C123 manual, section 2.3](https://www.ti.com/lit/ug/spmu296/spmu296.pdf) |
| The LaunchPad uses UART0 on PA0/PA1 | Confirmed for EK-TM4C123GXL. EK-TM4C1294XL uses jumper-selectable UART0/UART2 for ICDI. The user's board remains unconfirmed. [TM4C1294 manual, section 2.3.3](https://www.ti.com/lit/ug/spmu365c/spmu365c.pdf) |
| CCS 21.0.1 fixes the observed issue | Not established. TI's issue lists that fix release but still has unresolved/in-development status. It concerns missing displayed data, not shared-port access. [TI EXT_EP-13506](https://sir.ext.ti.com/jira/si/jira.issueviews:issue-html/EXT_EP-13506/EXT_EP-13506.html) |
| Kernel routing guarantees better performance | Unsupported. Kernel/user transitions are only one cost. Queue policy, timeouts, firmware, USB transport, and rendering must be measured. Asynchronous I/O also has overhead. [Microsoft I/O comparison](https://learn.microsoft.com/en-us/windows/win32/fileio/synchronous-and-asynchronous-i-o) |
| Virtual serial drivers must be kernel implementations | Incorrect as a general statement: Microsoft's VirtualSerial2 README identifies a UMDF 2 example, explicitly unsuitable for production as supplied. [Sample README](https://github.com/microsoft/Windows-driver-samples/blob/main/serial/VirtualSerial2/README.md) |
| VSPE is a kernel-based product | Its documentation identifies kernel virtual-device components plus a service. This does not establish the location or performance of every internal routing operation. [VSPE components](https://eterlogic.com/help/vspe/) |
| com0com/hub4com already demonstrates this topology | Confirmed. Windows 11 compatibility of an exact downloaded driver is still unverified. The upstream listing reports 2018-04-09 as its last update; that is not a finding about all forks. [hub4com](https://com0com.sourceforge.net/hub4com/ReadMe.txt), [project listing](https://sourceforge.net/projects/com0com/) |
| 115200/8-N-1 implies 11520 payload bytes/s | Correct theoretical per-direction UART ceiling: 115200 divided by 10 bits per byte. About 86.8 microseconds is wire time for one byte, not application latency. No supported maximum ICDI baud rate has been established here. |
| A signature proves Windows 11 compatibility | Incorrect. Signing policy, security configuration, architecture, and functionality require separate checks. The 2026 policy has audit/enforcement distinctions; do not assume uniform enforcement. [Windows Driver Policy](https://support.microsoft.com/en-us/windows/hardware/drivers/the-windows-driver-policy), [HVCI compatibility](https://learn.microsoft.com/en-us/windows-hardware/test/hlk/testref/driver-compatibility-with-device-guard) |
| Attestation/WDK guidance | Microsoft's current pages support the earlier qualifications about testing attestation and the dated VS/WDK combinations. Neither is a requirement to compile this user-mode executable. [Signing options](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings), [WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) |
| A user-mode project is better for a resume | An engineering recommendation about scope and demonstrable work, not a universal hiring fact. Attribute the external driver and report only actual measurements. |

**Additional local finding:** the installed TivaWare `uart_echo` has a nominal 1 ms delay inside its per-character receive interrupt loop and does not check `UARTCharPutNonBlocking` results. The library says failed nonblocking puts must be retried. Use this example only for a light smoke test, not as the throughput reference. Evidence: [installed example](C:/ti/TivaWare_C_Series-2.2.0.295/examples/boards/ek-tm4c123gxl/uart_echo/uart_echo.c:87), [library implementation](C:/ti/TivaWare_C_Series-2.2.0.295/driverlib/uart.c:1254). These machine-local links must be replaced or supplemented with reproducible version information in the eventual public project.

## 3. Resolve prerequisites without guessing

1. Read repository instructions and preserve unrelated work. At planning time, `main.c` is empty and untracked. Inspect it again before replacing or relocating it.
2. Record the actual LaunchPad model, firmware UART configuration, COM port, CCS patch version, and whether each terminal works alone. Request missing hardware information before flashing firmware. Host-side work can proceed without it.
3. Inspect the current Windows build and security settings. The planning machine reported Windows 11 Home, version `10.0.26200`, 64-bit. This is not a complete update-level or security-state report.
4. Confirm MSVC, a Windows SDK, and CMake are available in a development shell. `cl` and `cmake` were not found on the planning shell's PATH; this does not prove they are uninstalled. Use the compiler's C mode, not C++.
5. Record the installed virtual-port provider, version, architecture, original download URL, package hash, signing information, and configuration. Do not bundle proprietary binaries or copy third-party source without reviewing the exact license.
6. Use normal Windows security settings for acceptance. Do not make disabling Secure Boot, memory integrity, or driver policy a project setup step.

The planning queries returned no `Win32_SerialPort` entries. Recheck with Device Manager and device enumeration; an empty WMI result alone does not prove the board is physically absent. No physical-port reproduction or benchmark has occurred.

### Choose and qualify the virtual-port dependency

Start by evaluating **two VSPE Virtual Pair devices**, using the user's separately licensed installation if available. VSPE lists Windows 11 x64/ARM64 support; this remains a vendor claim until the exact package passes our tests. Do not use its built-in Splitter to perform the routing that this project is supposed to implement. [VSPE platform information](https://eterlogic.com/Products.VSPE.html)

For each Pair, use one lane; initially disable baud emulation, timeout overrides, forced delayed queuing, buffer overruns, and the setting that succeeds writes when the peer is absent. Enable peer connect/disconnect notification and record purge-on-open/close behavior. Validate rather than assume these settings preserve data. The vendor documents EV_EVENT1/EV_EVENT2 notifications for remote opens/closes. [VSPE Pair settings](https://eterlogic.com/help/vspe/DevicePairPage.html)

Keep provider-specific peer notifications behind `provider_vspe.c`. They are not portable Windows guarantees. A generic pair mode can support controlled sessions, but cannot claim reliable peer-presence detection unless proven for that provider. Never interpret a zero-byte serial read as socket EOF.

If VSPE is unavailable, evaluate a maintained pair provider, or a specific com0com package that passes the same tests. Do not silently choose an unsigned package, buy a license, or replace this assignment with hub4com. Continue with mock transport tests while the dependency is unresolved. Custom virtual-device development requires a separate plan.

## 4. Required topology and behavior

Example port numbers are placeholders; allocate unused names and record the actual mapping.

```text
Tiva UART ↔ ICDI ↔ COM5 physical ↔ portsplitter.exe
                                      │       │
                               opens COM20   opens COM22
                                      ↕       ↕
                               pair: COM21   pair: COM23
                                      ↕       ↕
                               CCS terminal  PuTTY terminal
```

The executable opens COM5, COM20, and COM22. CCS opens COM21; PuTTY opens COM23. It never opens the application-facing ends. Do not connect both terminals to COM5 or one shared virtual endpoint.

Implement these routing rules:

- Physical reads are copied, in order, into both destination queues.
- Reads from the selected writer's router-facing endpoint are queued to the physical port.
- Reads from the other endpoint are consumed and discarded, with a counter. This prevents accidental typing or terminal responses from reaching the board. The terminal's write may still succeed locally; do not claim OS-enforced read-only access.
- Do not mirror outbound commands into another terminal's received stream. If firmware echoes them, distribute that echo like any other received data.
- Preserve NUL, CR/LF, high-bit bytes, and escape sequences. Do not use string functions on payloads, add line endings, or infer message boundaries.
- Physical baud/framing/control-line settings belong to the executable's startup configuration. Changes made by CCS/PuTTY affect their virtual endpoints; they are not automatically forwarded to ICDI.
- No automatic DTR/RTS, break, purge, or modem-status propagation across pairs to the physical device in version 1. Explain this limited UART-console contract in the README.

**Delivery scope:** bytes observed during a RUNNING session with both clients ready. Firmware/USB/driver losses before our reads remain possible; detect them with sequence tests and available error indicators. A successful driver write is not proof the terminal consumed the bytes.

## 5. Proposed CLI and repository layout

Implement the following interface; the commands below describe future functionality and have not been run:

```powershell
portsplitter.exe --help
portsplitter.exe list
portsplitter.exe run --source COM5 --client-a COM20 --client-b COM22 --writer a --baud 115200 --provider vspe --queue-bytes 65536 --write-timeout-ms 5000 --wait-for-start
```

Required `run` options: source, both client endpoints, writer (`a`, `b`, or `none`), baud, and provider (`vspe` or `generic`). Fix framing at 8-N-1 and flow control None for version 1; reject unsupported framing requests rather than pretending to honor them. Provide explicit `--dtr on|off` and `--rts on|off` settings, default off for the physical ICDI UART profile; verify that profile on the actual board. Make source startup discard opt-in with `--discard-source-input-on-start`.

The start barrier is enabled by default; `--wait-for-start` makes it explicit. Defer unattended startup. For automation, let the harness release the barrier through a documented control-input mechanism. Define exit codes: 0 for an intentional stop without a recorded session fault, 2 for configuration errors, 3 for startup/device errors, and 4 for runtime integrity/transport faults. Exit 0 does not assert delivery of outstanding bytes canceled on stop; the final summary must show them.

Normalize `COMn` and `\\.\COMn` names, including COM10 and higher. Reject zero/negative/out-of-range numeric options, unknown options, and duplicate endpoint names before opening anything. Verify the pair mapping separately; unique names do not prove that the correct endpoints were chosen.

Use a project queue limit of 4 KiB to 16 MiB per queue, with 64 KiB default. Validate integer arithmetic before allocation. Reject baud values that do not fit a positive `DWORD`; actual device support is determined by configuration success and testing. The 5-second write timeout is a starting policy for the initial 115200-baud fixture. At lower baud rates, require a timeout/chunk combination long enough for the wire time plus margin, or reduce the chunk size; do not misclassify slow valid transmission as a stuck driver.

Suggested files, relative to the repository:

| File | Responsibility |
| --- | --- |
| `CMakeLists.txt`, `CMakePresets.json` | Build C17 executable and tests; expose Debug/Release presets. |
| `src/main.c`, `src/config.c/.h` | Parse options, validate configuration, establish lifecycle. |
| `src/serial_win32.c/.h` | Own handles, configuration, asynchronous operations, errors, cancellation. |
| `src/router.c/.h` | Own routing decisions and state transitions; testable through a small transport interface. |
| `src/byte_queue.c/.h` | Fixed-capacity FIFO storage with explicit overflow results. |
| `src/provider_vspe.c/.h` | Interpret validated provider notifications; keep this out of generic routing. |
| `src/metrics.c/.h` | Counters and timing snapshots; no payload logging by default. |
| `tests/` | Queue, routing, configuration, completion, and fault tests. |
| `tools/serial_test.c` | C integration harness: source simulator, readers, echo client, and timing capture. |
| `firmware/` | Optional board-specific measurement fixture, only after confirming the model. |
| `docs/SETUP.md`, `docs/VALIDATION.md` | Reproduction commands, exact provider configuration, results and limitations. |

Use `/W4`, treat warnings in project code as errors, and run static analysis where supported. Compile `.c` files as C. Use fixed-width integer types and checked size conversions. Do not add a C++ runtime, GUI toolkit, or serial wrapper library for this version.

## 6. Implement in this order

### Step 1 — Build the skeleton and enumerate devices

**Purpose:** make the program reproducible and let the user identify the correct port without opening it.

Create the CMake targets and CLI validation. Implement `list` using SetupAPI and `GUID_DEVINTERFACE_COMPORT`; report available device path, friendly name, COM name, and device instance identity. Explicit COM arguments remain useful if a provider lacks expected enumeration metadata. Do not select the first matching board automatically. Microsoft's recommended discovery interface is [GUID_DEVINTERFACE_COMPORT](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/guid-devinterface-comport).

**Completion check:** build Debug and Release; help/list work without hardware; invalid arguments fail without touching devices.

### Step 2 — Implement serial opening and configuration

**Purpose:** obtain exclusive ownership and eliminate inherited serial settings that could alter or suppress bytes.

Use `CreateFileW` with `GENERIC_READ | GENERIC_WRITE`, sharing mode zero, `OPEN_EXISTING`, and `FILE_FLAG_OVERLAPPED`. Open every required endpoint successfully before declaring the router ready. On failure, print the API, port, numeric error, and formatted system message; release resources already acquired. Do not describe every access-denied result as conclusively another terminal's lock. [Communications handles](https://learn.microsoft.com/en-us/windows/win32/devio/communications-resource-handles)

Initialize the `DCB` length, retrieve existing settings with `GetCommState`, and deliberately apply the supported configuration. Set baud, 8 data bits, no parity, one stop bit, binary mode; disable XON/XOFF, CTS/DSR gating, DSR sensitivity, NUL stripping, parity-byte replacement, and automatic RTS/DTR handshaking. Set `fAbortOnError` false and monitor errors explicitly. Preserve valid unrelated fields. The source and virtual endpoints have separate state. [DCB fields](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-dcb), [SetCommState](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setcommstate)

Apply and read back timeouts. For an initial read strategy, use `ReadIntervalTimeout = MAXDWORD`, `ReadTotalTimeoutMultiplier = MAXDWORD`, and `ReadTotalTimeoutConstant = 250`. This documented combination returns available data or waits for the first byte, with an idle timeout; it does not deliberately hold arriving data for 250 ms. Use write multiplier zero and the configured write constant. These are starting values to validate with both drivers, not guaranteed optimal settings. [COMMTIMEOUTS](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts)

**Completion check:** the integration harness can open a pair, transfer binary data, time out while idle, and stop. Verify the physical configuration separately when the board is connected.

### Step 3 — Implement the asynchronous operation engine

**Purpose:** keep one blocked destination from blocking the thread that serves all other ports.

Use one event-loop thread for the three port handles, with one outstanding read and one outstanding write per handle. Allocate a separate persistent `OVERLAPPED`, manual-reset event, and owned buffer for each operation. Start with 4096-byte buffers. Keep buffers and operation structures alive until completion. [Asynchronous I/O](https://learn.microsoft.com/en-us/windows/win32/fileio/synchronous-and-asynchronous-i-o)

Use `WaitForMultipleObjects` with wait-any semantics, the stop event, active operation events, and an optional provider-notification event per virtual handle. Include a finite deadline for fault checks. Check stop first; process ready completions fairly so a continuously ready source cannot starve writers. Never wait on an idle operation's already-signaled event. [WaitForMultipleObjects](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects)

Define one completion path for each operation. Handle immediate success exactly once; treat `ERROR_IO_PENDING` as pending, not failure. On a signaled pending operation, use `GetOverlappedResult(..., FALSE)`; handle `ERROR_IO_INCOMPLETE` without freeing anything. Reset/reinitialize event state only after harvesting completion and before reissuing. [GetOverlappedResult](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult)

Route only the completed byte count. A zero-byte read is idle/timeout behavior, not disconnect. Detect repeated immediate zero-byte completions as a timeout/driver compatibility problem rather than spinning. [ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)

Keep a dedicated transmit buffer and offset per pending write. Preserve its unsent suffix after a successful short completion. Do not consume or overwrite unsent data. Treat zero progress or a write exceeding the configured deadline as a fault. After a failed or canceled physical write, do not automatically retry the whole command: some bytes may already have reached the board. [WriteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile)

Use `WaitCommEvent` only for required status/provider events, not as a substitute for reads. Give it its own persistent mask storage and `OVERLAPPED`. Generic event definitions do not establish that a virtual peer closed. [WaitCommEvent](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-waitcommevent)

**Completion check:** mock immediate/pending/partial/zero/error outcomes, plus simultaneous read/write completion. Assert no double completion, use-after-free, or idle spin.

### Step 4 — Implement queues and the routing contract

**Purpose:** retain independent delivery progress while bounding memory.

Allocate three byte queues: source-to-A, source-to-B, and selected-writer-to-source. Start at 64 KiB each, configurable within checked limits. Allocate once at startup. The event loop owns them, so lock-free algorithms and queue mutexes are unnecessary in version 1.

For each physical read, reserve capacity in both receive queues before appending to either. Otherwise fault the session; do not send the new block to only one client and silently omit the other. Offer queued bytes immediately to idle writers; do not wait for a full buffer or line terminator. Cap work per event-loop turn to retain fairness.

Drain the observer's incoming data into a discarded-input counter. Do not inject warnings into either serial stream. Commands from the selected writer remain ordered; the router does not parse them or prove their application-level success.

**Overflow policy for version 1: stop with an explicit failure.** Independent queues absorb temporary stalls, but persistent overflow or write timeout terminates the session. Do not drop-oldest, silently overwrite, grow memory indefinitely, or claim indefinite slow-reader isolation. A later observer-disable mode may preserve controller availability, but needs separate delivery semantics.

**Completion check:** deterministic tests cover queue wraparound, exact capacity, asymmetric reader speeds, binary payloads, writer selection, ignored observer input, and overflow without silent partial broadcast.

### Step 5 — Define session startup, peer lifecycle, and shutdown

**Purpose:** prevent stale commands and misleading success across disconnected sessions.

Use states `CONFIGURING → WAITING_FOR_START → RUNNING → STOPPING → STOPPED/FAILED`. Require both terminals disconnected when launching a fresh session. Open/configure the router-side virtual endpoints first; arm validated provider notifications before the terminals connect. Display the mapping and wait for the user to connect both terminals and explicitly start. Continue harvesting and rearming peer events during this wait, and make it interruptible by the stop event. In the VSPE profile, require observed connection events for both peers and no outstanding disconnected state before accepting start; do not infer presence from successful router-side opens. Open/configure the physical port only at start. Do not accept terminal commands before RUNNING.

Before RUNNING, clear pre-start input queued on the router-side virtual handles and reset router counters/queues. Qualify the provider's purge behavior so stale peer-side data cannot masquerade as a new session. Purge source input only when the explicit discard flag is supplied, and label it an intentional startup discard. Never purge a running source as a speed optimization. `PurgeComm` discards bytes; it is not a successful-delivery operation. [PurgeComm](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-purgecomm)

For the validated VSPE profile, a peer disconnect during RUNNING ends the session. A disconnect followed rapidly by a reconnect still invalidates it. Do not automatically re-arm or forward buffered commands after reconnect. Start a fresh process/session with clean pairs. For generic providers without peer notifications, document that peer closure might be detected only later through timeout/overflow, or not at all; do not award the same lifecycle acceptance result.

Periodically call `ClearCommError` on owned handles and after relevant errors. Record reported overflow, framing, and parity conditions and mark the run failed; these flags are not exact lost-byte counts. Not every driver reports every upstream error. [ClearCommError](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-clearcommerror)

On Ctrl+C, source removal, write deadline, queue overflow, or peer loss: stop submitting new work, request `CancelIoEx` on pending operations, and collect final completions before releasing their memory/events/handles. Cancellation races may finish successfully or report an abort; `ERROR_NOT_FOUND` from cancellation does not prove that completion was already harvested. [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)

Do not use `TerminateThread`, replay queued commands on restart, or assume cancellation always succeeds. Keep a diagnostic deadline; if a provider never completes canceled requests, report it as a provider failure without freeing live I/O storage. Do not promise a bounded shutdown for a broken driver. [Cancellation considerations](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)

Report read bytes, write-completed bytes, queued/in-flight bytes at stop, ignored observer bytes, and fault reason. A canceled partial transmission has uncertain delivery; do not label it delivered or unsent with false precision. Never call synchronous `FlushFileBuffers` on the forwarding loop to imply terminal consumption.

**Completion check:** repeated start/stop, terminal closure, source removal, cancellation races, and startup failures release all resources on validated drivers. Restart requires an explicit new session.

### Step 6 — Add metrics without obstructing forwarding

**Purpose:** demonstrate performance and explain failures with evidence.

Maintain per-direction byte counts, queue high-water marks, partial/zero completions, ignored observer input, and error codes. Use `QueryPerformanceCounter`/frequency for elapsed timings. Measure request-to-matching-response round trips on one host clock; do not subtract unrelated board and host clocks. [High-resolution timestamps](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)

Keep console/disk writes out of steady-state forwarding. A bounded telemetry queue may feed a logger thread; telemetry overflow increments a separate counter and never alters payload. A simpler first implementation may emit only startup information and a final summary. Diagnostics must remain separate from the serial byte stream.

**Completion check:** redirected or slow diagnostic output does not block payload forwarding. Final metrics reconcile completed, queued, and uncertain work without claiming application consumption.

## 7. Tests to implement and run

### Automated tests without a driver

Use a small injectable transport interface to script completions and time progression. Test externally meaningful behavior, not only helper methods: two identical receive streams, byte order across partial writes, observer commands never reaching the source, no replay after faults, fair progress under simultaneous completions, bounded memory, and cancellation ownership. Add parameter-validation and queue wraparound tests. A skipped hardware test must not be reported as passed.

### Provider qualification and integration tests

First test each real virtual pair bidirectionally with the C harness. Verify enumeration, exclusive opens, 8-N-1 configuration, binary bytes, idle timeout behavior, cancellation, peer notifications, absent-reader behavior, full buffers, and both ends reopening. Confirm the settings do not acknowledge discarded data as successful delivery.

For an automated splitter test, create a third pair as the physical-source substitute. The simulator opens one side, the router opens the other; two harness clients open A/B. Compare complete numbered payloads at both readers, while the selected writer sends a distinct stream to the simulator. Repeat with each writer selection and with writer `none`. Include NUL, XON/XOFF byte values, and all byte values 0–255.

Run stalled-reader tests long enough to exceed provider buffers plus router capacity, or use a controlled small test capacity. Expect the declared timeout/overflow failure, not silent data loss. Test an observer flooding input while normal source/writer traffic continues. Exercise source removal and peer disconnect during pending I/O. Record exact provider settings and results in `docs/VALIDATION.md`.

**Gate:** if the provider silently loses data, ignores required controls, or cannot complete cancellation, the complete system is not validated. Fix configuration, select another provider, or report the limitation. Mock success cannot override this result.

## 8. CCS/PuTTY and LaunchPad acceptance

1. Confirm the board model and connector. Use the ICDI debug USB connection for the documented UART path; the separate user USB connector requires suitable USB firmware.
2. Establish a direct baseline with each terminal independently. If CCS alone fails, investigate its version and configuration before evaluating the splitter.
3. In CCS 21 use **View → Console → Serial Console**. Control **Local Echo** and **Send Characters on Enter** deliberately; local echo is not proof of board reception. [CCS 21 documentation](https://software-dl.ti.com/ccs/esd/documents/users_guide/ccs_debug-main.html#serial-console-view)
4. Configure PuTTY for Serial and the application-facing B port; configure CCS for the application-facing A port. Use 115200/8-N-1/no flow control only when firmware matches. [PuTTY serial settings](https://the.earth.li/~sgtatham/putty/0.85/htmldoc/Chapter4.html#config-serial)
5. Start the router using the recorded router-facing ports, connect both terminals, then release the start barrier. Use a board-generated sequence or known harmless command. Verify matching board output and that observer input cannot control the board.
6. Test with a CCS debug session active and firmware running. A debugger halt may stop firmware output; do not classify expected silence as splitter failure.
7. Close a terminal and unplug the board in separate trials. Confirm the declared session failure and a clean explicit restart, with no queued command replay.

Do not flash test firmware over the user's program without confirming the intended target and that replacement is authorized. If a separate fixture is needed, create it in `firmware/` for review first.

### Measurement fixture requirements

Use the TivaWare UART configuration as a reference, but remove per-character delays from the measurement path. Implement short receive interrupts and bounded transmit buffering, with explicit overflow counters and handling for failed nonblocking writes. Keep payload generation, formatting, and LED delays outside the receive interrupt. Validate the fixture directly before attributing losses to the splitter.

Provide two test modes: paced numbered output with integrity checking, and identifiable echo requests for round-trip timing. Define the fixture format in its README; it is test traffic, not a new protocol the splitter must understand. Keep arbitrary user firmware working unchanged for ordinary routing.

## 9. Performance evaluation and acceptance criteria

Start at the board's verified baud rate. Do not raise it beyond the proven firmware/bridge combination or treat the MCU's UART specification as the ICDI bridge's limit.

Measure the direct connection and the required two-client router configuration. An optional one-destination microbenchmark may isolate duplication costs in the test harness; label it separately and do not weaken the production two-client readiness contract to obtain it. Keep firmware, baud, payload, terminal/reader settings, power configuration, and logging comparable. Distinguish an automated capture run from a UI compatibility demonstration.

Initial test budget: three repetitions of a 10-minute sequence run at a measured sustainable offered rate, plus at least 1000 identifiable echo requests per configuration. These are project test choices, not a reliability guarantee. Run a rate sweep up to the **observed direct-path sustainable rate**, respecting the 8-N-1 theoretical ceiling `baud / 10`.

Report throughput per reader, missing/duplicate/out-of-order bytes or frames, p50/p95/p99/max observed round-trip latency, CPU, memory, queue high-water marks, and errors. Source-to-driver-completion timing is internal forwarding timing, not end-to-end latency. A finite maximum observation is not a worst-case guarantee.

Acceptance requires:

- Both CCS and PuTTY can receive the same board data through separate virtual ports.
- The selected writer controls the board; observer transmission is suppressed and counted.
- Automated healthy-session tests have zero detected gaps, duplicates, or reorderings at the recorded offered load.
- No unbounded memory growth or idle busy loop; startup, faults, and shutdown follow the documented contract.
- All losses, timeouts, disconnections, and unsupported provider behavior are reported honestly.
- Measured overhead is published relative to the direct baseline. Set any numerical latency target from that baseline and the user's needs before declaring a performance pass; do not invent a sub-millisecond guarantee.

If performance is inadequate, profile first. Investigate driver settings and read completion behavior, then chunk sizes and telemetry. Re-run integrity tests after every performance change. Introduce IOCP, additional forwarding threads, or custom drivers only when measurements justify their complexity.

## 10. Final handoff checklist

- [ ] C17 host source, build presets, reproducible dependencies, and warning-clean builds.
- [ ] Unit/fault tests passing; integration results explicitly identified as passed, failed, or not run.
- [ ] Exact virtual pair configuration and source/client COM mapping documented.
- [ ] Windows build, security state, provider version, CCS/PuTTY versions, board, and firmware recorded.
- [ ] Two-terminal hardware demonstration completed, or clearly marked pending with the exact missing dependency.
- [ ] Performance report contains raw results and configuration, with no unsupported fastest/lossless claims.
- [ ] README explains fixed writer ownership, control-operation limits, fail-on-overflow policy, and restart procedure.
- [ ] External virtual driver credited; no claim that this project created that driver.
- [ ] No automatic reconnection, hidden data dropping, driver-policy changes, or unrequested firmware replacement.

In the implementation report, distinguish **code complete**, **provider validated**, and **hardware validated**. They are separate outcomes.
