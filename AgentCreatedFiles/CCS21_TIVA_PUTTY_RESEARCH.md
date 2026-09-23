# CCS 21 and PuTTY sharing a Tiva LaunchPad on Windows 11

Research date: September 22, 2026. This narrows the [general splitter research](WINDOWS_11_SERIAL_SPLITTER_RESEARCH.md) to the reported problem. It identifies a first solution to evaluate, not a verified fix or a performance guarantee.

The [C implementation handoff](IMPLEMENTATION_PLAN.md) now specifies our own routing engine with external virtual **pairs**. The VSPE **Splitter** below remains a reference solution to evaluate; it is not the component that will implement our C project's routing.

## Likely explanation

If CCS Serial Console and PuTTY each work separately, but the second program cannot open the same COM port, the likely cause is exclusive ownership. Windows documents serial communications handles as exclusive. Matching baud rates, launching as administrator, or changing COM numbers does not provide a broadcast mechanism. [Microsoft: Communications Resource Handles](https://learn.microsoft.com/en-us/windows/win32/devio/communications-resource-handles)

This does not mean that CCS debugging and PuTTY are inherently incompatible. On the EK-TM4C123GXL, the ICDI enumerates as both a debugger and a virtual COM port; the UART connection uses PA0/U0RX and PA1/U0TX. Debugging and serial terminal access are distinct interfaces. [TI: TM4C123G LaunchPad manual, section 2.3](https://www.ti.com/lit/ug/spmu296/spmu296.pdf)

The exact board is still unconfirmed. The EK-TM4C1294XL also exposes an ICDI virtual COM port, with jumpers selecting UART0 or UART2. Do not apply the TM4C123 pin description indiscriminately. [TI: Connected LaunchPad manual, section 2.3.3](https://www.ti.com/lit/ug/spmu365c/spmu365c.pdf)

## Local observations and limits

- A `C:\ti\ccs2101` installation directory exists, with CCS 21.0.1 manifest material. This does not prove which version was running when the problem happened.
- TivaWare 2.2.0.295 is installed. Its `examples\boards\ek-tm4c123gxl\uart_echo\uart_echo.c` documents and configures UART0 at 115200 baud, 8 data bits, no parity, one stop bit. That is an example configuration, not a measurement of the user's firmware.
- `Win32_SerialPort` returned no entries during this investigation. No currently usable LaunchPad COM port was identified, so the fault could not be reproduced and no throughput or latency was measured.
- No firmware, drivers, or terminal settings were changed.

Later source inspection found that the installed `uart_echo` example delays about 1 ms per received character and ignores nonblocking transmit failure. It is suitable for a light connectivity check, not a sustained-throughput reference. The implementation handoff requires a separately validated measurement fixture. An empty `Win32_SerialPort` result also does not prove physical absence; check device enumeration before diagnosing missing hardware.

## Distinguish three symptoms first

| Symptom | Interpretation and next check |
| --- | --- |
| Both work alone; second terminal cannot open | Strong evidence for exclusive COM ownership. Test a splitter with separate virtual ports. |
| PuTTY works alone; CCS opens but displays nothing | Investigate CCS version, settings, and serial-console behavior before blaming sharing. |
| Neither works alone | First establish a working physical connection: correct USB connector, port/driver, UART firmware, baud rate, and running target. |

TI issue EXT_EP-13506 describes a serial console that sometimes displays no data although an external console does. It lists “Found In Release: CCS_20.5.0” and “Fix In Release: CCS_21.0.1,” while its visible status remains unresolved/in development. Treat this as a relevant issue record, not proof that it caused this incident or that every related case is fixed. It does not remove Windows port exclusivity. [TI: EXT_EP-13506](https://sir.ext.ti.com/jira/si/jira.issueviews:issue-html/EXT_EP-13506/EXT_EP-13506.html)

CCS 21.0.1 documents **View → Console → Serial Console**, a connect/disconnect control, and options for **Local Echo** and **Send Characters on Enter**. A character visible through local echo is not evidence that the board received it; waiting for Enter is also different from transport latency. [TI: CCS 21.0.1 Serial Console](https://software-dl.ti.com/ccs/esd/documents/users_guide/ccs_debug-main.html#serial-console-view)

## First solution to evaluate

Use one owner of the LaunchPad's physical COM port and two virtual COM endpoints, one per terminal. Begin with a single writer and two readers, unless both programs truly need command ownership.

```text
LaunchPad UART ↔ ICDI USB serial ↔ physical COM port
                                           ↕
                                    local splitter
                                      ↙        ↘
                          virtual COM A      virtual COM B
                          CCS Serial Console PuTTY
                          read/write         receive only initially
```

The writer can be PuTTY instead; the assignment is a provisional testing choice. Both receive the board's output. An observer sees typed commands only if the firmware echoes them or a separately specified mechanism mirrors transmitted data.

**Concrete baseline: VSPE Virtual Splitter.** Its vendor lists Windows 11 x64/ARM64 support with a required license. It is a practical first compatibility candidate; no reviewed benchmark establishes it as the fastest. [Eterlogic: supported platforms](https://eterlogic.com/Products.VSPE.html)

The vendor documents separate target ports, per-target send permissions, and modem-line redirection. Its default receive behavior can let the slowest reader stall the source. “Not allowed to block read flow” prevents that stall but may cause that client to miss data. “Buffered Read Mode” can reduce CPU use while adding delay. [Eterlogic: Virtual Splitter behavior and settings](https://eterlogic.com/help/vspe/DeviceSplitterPage.html)

### Setup sequence when the board is available

1. Record the exact board, CCS patch version, physical COM number, and firmware serial settings. Use Device Manager to identify the LaunchPad port.
2. Disconnect CCS Serial Console. Open PuTTY on the physical port and verify reception and a harmless command if supported. Close PuTTY, then repeat in CCS alone. Record the exact error if either fails.
3. For a normal ICDI UART echo example, use 115200/8-N-1 with flow control None. For other firmware, use its actual settings. PuTTY exposes these under Connection → Serial. [PuTTY: Serial configuration](https://the.earth.li/~sgtatham/putty/0.85/htmldoc/Chapter4.html#config-serial)
4. Evaluate a current compatible splitter package with Windows security protections enabled. Close both physical-port sessions before starting the splitter.
5. Configure the source as the physical port and create two unused virtual COM names. Connect CCS to one and PuTTY to the other. Neither terminal should continue targeting the physical port.
6. Initially permit transmission from only the chosen controller. Avoid forwarding observer control-line changes. For a latency-oriented comparison, start without buffered-read mode and compare CPU use and latency before retaining that choice. These options are documented in the [VSPE splitter settings](https://eterlogic.com/help/vspe/DeviceSplitterPage.html).
7. Verify identical board output in both terminals and command responses from the controller. Disconnect/reconnect each terminal independently, then test a deliberately slow reader.

Do not silently select data loss to make a speed result look better. Decide explicitly whether the observer may miss data when stalled. If both terminals must preserve every byte, sufficient buffering and sustainable consumer rates are acceptance conditions.

## What “top performance” can honestly mean here

No method can be guaranteed fastest before measuring the board, USB bridge, firmware, Windows driver, splitter, and terminal behavior together. Choosing kernel mode or a particular programming language alone does not establish superior end-to-end results.

For 8-N-1 UART, each payload byte occupies ten transmitted bits. At 115200 baud, the ideal ceiling is therefore **11520 payload bytes/second per direction**, before idle gaps and other bottlenecks. One byte takes about **86.8 microseconds on the UART wire**; that is not a prediction of USB or terminal latency. A splitter duplicates the received stream; it does not double the board's output capacity. These figures are arithmetic for the stated framing, not benchmarks.

The performance objective for this workload should be: preserve the offered byte stream, keep interactive response delay close to the direct-port baseline, and prevent an observer from unexpectedly freezing the controller. CPU and memory consumption matter alongside latency.

If custom implementation becomes necessary, investigate asynchronous source I/O, prompt forwarding of available bytes, independent bounded output queues, and keeping display/logging work out of the forwarding path. These are engineering candidates, not experimentally proven choices for this system. Windows `COMMTIMEOUTS` affects when reads complete; zero timeout values do not universally mean immediate delivery. [Microsoft: COMMTIMEOUTS](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts)

### Measurements that can justify a choice

| Test | Evidence to collect |
| --- | --- |
| Direct physical-port baseline | Numbered payload received without gaps; request/echo round-trip latency from one host clock. |
| Splitter with one reader, then two | Loss, duplication, ordering, CPU use, memory, and latency relative to the baseline. |
| Sustained traffic and short bursts | Highest observed lossless rate at the unchanged baud setting; p50/p95/p99 and maximum observed latency. |
| Stalled observer | Whether the controller stalls; queue growth and explicit loss/error behavior. |
| Reconnect and debug activity | What happens after terminal close, USB reconnect, board reset, and target halt/resume. |

Use a repeatable host-side capture/echo harness for timing and the actual terminals for application compatibility. UI repaint timing alone is not a transport benchmark. Compare the same firmware, payload, baud, security settings, and logging configuration across repeated runs. A maximum observed delay is not a hard worst-case guarantee.

## Alternatives if this baseline is insufficient

- **com0com plus hub4com:** already documents real-port sharing with virtual pairs. Useful as another implementation to compare, but current Windows 11 driver compatibility must be established first. [Upstream hub4com README](https://com0com.sourceforge.net/hub4com/ReadMe.txt)
- **Custom virtual endpoints and forwarding:** retains unchanged serial clients but introduces driver compatibility and deployment work. Select UMDF/KMDF only after identifying a demonstrated requirement or measured bottleneck; see the general research note.
- **A separate USB device serial channel:** TI points to TivaWare's `usb_dev_serial` example for the TM4C123 user USB connector. This requires appropriate firmware and does not automatically mirror the ICDI UART. It changes the application architecture and is not a transparent fix for two terminals opening one port. [TI: USB device versus debug COM interface](https://e2e.ti.com/support/microcontrollers/arm-based-microcontrollers-group/arm-based-microcontrollers/f/arm-based-microcontrollers-forum/1229589/ek-tm4c123gxl-usb-device-not-usb-debug-port-for-com-interface)

Pending inputs: exact LaunchPad model; whether each terminal works alone; the failing error; current UART settings; and whether both terminals must transmit. The immediate next milestone is a reproduced diagnosis and successful two-terminal reception, followed by measured performance—not an unsupported fastest-method claim.
