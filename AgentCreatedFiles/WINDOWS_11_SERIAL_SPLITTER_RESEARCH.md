# Virtual serial port splitter research for Windows 11

Research date: September 22, 2026. Status: exploratory research, not an approved implementation plan.

For the specific reported CCS 21, PuTTY, and Tiva LaunchPad issue, see [the focused investigation and initial setup](CCS21_TIVA_PUTTY_RESEARCH.md).

The subsequent [C implementation plan](IMPLEMENTATION_PLAN.md) contains an accuracy audit and defines a first version using an existing virtual-pair driver plus our own C routing engine. This research note continues to describe broader alternatives.

This note examines established Windows approaches to exposing one serial connection to multiple applications. It separates documented behavior, vendor claims, and engineering questions. No driver was installed, sample compiled, or hardware experiment performed for this research. Consequently, no option below is claimed to have been validated on this project's Windows 11 system.

## 1. What problem is being solved?

Windows normally opens a communications resource exclusively: Microsoft's documentation requires a zero sharing mode in `CreateFile`, and an open fails when another process already uses the resource. This behavior predates Windows 11; it is not a new Windows 11 restriction. A splitter must provide a sharing mechanism above or within the serial-device implementation. [Microsoft: Communications Resource Handles](https://learn.microsoft.com/en-us/windows/win32/devio/communications-resource-handles)

Several different requirements can be described as “splitting”:

| Requirement | Meaning | Question to resolve |
| --- | --- | --- |
| Receive duplication | Every application receives a copy of incoming data. | Is the device a continuous broadcaster? |
| One controller, multiple observers | One application sends commands; others receive traffic. | Must observers receive transmitted commands as well as device responses? |
| Multiple controllers | Several applications send commands to one device. | Can the device protocol support this without confusing responses or state? |
| Virtual null-modem pair | Two software endpoints exchange serial data. | Is a physical source port needed at all? |
| Shared port name | Several applications open the same virtual COM name. | Is this required, or can each application select a different virtual COM port? |

These are alternatives to investigate, not equivalent promises. In particular, successfully opening two virtual ports does not establish that two independent applications can safely control the attached device.

## 2. Established options

### A. Evaluate an existing splitter

Eterlogic's VSPE documentation describes a Virtual Splitter that connects multiple virtual COM ports to an existing port. Its product page lists Windows 11 x64 and ARM64 support and indicates that those platforms require a license. These are vendor statements, not independent validation of a particular package or Windows update level. [Eterlogic: VSPE product and support information](https://eterlogic.com/Products.VSPE.html)

VSPE also documents a driver, a Windows service, and application/API components. This provides a concrete example of an existing product dividing virtual-device support and management across components. [Eterlogic: VSPE documentation](https://eterlogic.com/help/vspe/)

**Research value:** use an existing implementation as a behavioral reference and determine whether the actual requirement already has an adequate solution.

**Questions:** Does the exact release load with the target security settings? How does it arbitrate writes and port settings? Can it run unattended? Does its API support the intended integration? What installation, redistribution, and support rights would be needed? An end-user license should not be assumed to grant SDK or redistribution rights.

### B. Study com0com with hub4com

The upstream com0com project provides virtual serial ports and identifies its license as GPLv2. Its project listing reports a last update of April 9, 2018; that is a caution about the examined upstream distribution, not proof that all forks are inactive. [com0com: upstream project](https://sourceforge.net/projects/com0com/)

The upstream hub4com README explicitly documents sharing a real serial device among applications. Its GPS example creates two virtual port pairs, connects the hub to their internal endpoints and a physical port, duplicates incoming data to both applications, and routes return data from one application to the physical port. This is an existing documented arrangement, not a newly proposed invention. [hub4com: upstream README, GPS hub example](https://com0com.sourceforge.net/hub4com/ReadMe.txt)

**Research value:** inspect an existing separation between virtual port creation and a routing application. A port-pair driver alone is not the complete splitter in this arrangement.

**Unresolved:** the reviewed upstream material does not establish compatibility of its downloadable driver with current Windows 11 security policy, memory integrity, or ARM64. Do not treat a package labeled “signed” or a report from an older Windows release as sufficient evidence. Review exact source and binary provenance, included licenses, and maintenance before considering reuse.

### C. Investigate a custom driver using Microsoft's samples

Microsoft's `VirtualSerial2` sample demonstrates a virtual COM port using **UMDF 2**, the User-Mode Driver Framework. Its README explicitly identifies it as a minimal demonstration unsuitable for production use. Thus, “a virtual COM port must be implemented entirely in kernel mode” is too broad a claim. However, the sample is not evidence of a complete multi-client splitter or compatibility with every serial application. [Microsoft: VirtualSerial2 README](https://github.com/microsoft/Windows-driver-samples/blob/main/serial/VirtualSerial2/README.md)

The sample's installation template uses the Ports device class and includes installation metadata. Studying that package is part of understanding how the virtual device is presented to Windows, beyond simply moving bytes. [Microsoft: VirtualSerial2 installation template](https://github.com/microsoft/Windows-driver-samples/blob/main/serial/VirtualSerial2/ComPort/virtualserial2um.inx)

Microsoft also supplies a WDF serial sample based on the inbox `Serial.sys` driver. It is a hardware-oriented reference, not a ready-made splitter. [Microsoft: Serial driver samples](https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/serial-driver-samples)

**Research value:** determine whether UMDF 2 meets the required device behavior before choosing a kernel-mode implementation. A custom KMDF implementation remains an option to investigate if concrete requirements justify it; this research does not establish that it is necessary or faster.

**Unresolved:** required serial requests, per-client state, physical-port access, device creation/removal, routing boundaries, error propagation, deployment, and long-term maintenance. Using UMDF does not eliminate driver-package installation and signing questions.

### D. Consider application-level sharing if clients can change

If every consuming application can be modified, a single Windows process could own the physical port and distribute data through a supported IPC interface, such as named pipes. This is an architectural possibility inferred from exclusive ownership, not an implemented solution in this project.

It would not by itself expose a serial-compatible `COMx` endpoint to an unchanged legacy application. If such applications are required, an existing virtual-port component or driver investigation remains relevant. VSPE's documented bridges between serial, TCP, and named-pipe endpoints demonstrate that these transport combinations already exist. [Eterlogic: VSPE documentation](https://eterlogic.com/help/vspe/)

### A related technology that should not be mistaken for a splitter

Microsoft's SerCx2 documentation concerns serial controllers and peripherals. Its published device-interface approach does not assign legacy COM numbers. It should not be selected merely because it is a newer serial framework; the reviewed documentation does not establish it as a drop-in virtual COM splitter. [Microsoft: SerCx/SerCx2 device-interface publication](https://learn.microsoft.com/en-us/windows-hardware/drivers/serports/device-interface-publication-sercx)

## 3. Windows 11 deployment considerations

### Driver trust is a separate compatibility dimension

Microsoft states that virtual kernel drivers have the same signing requirements as physical-device drivers. Older documentation describes Microsoft signing through certification or attestation, but should be read alongside newer policy material. [Microsoft: Kernel-mode code-signing requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-requirements--windows-vista-and-later-)

The current Windows Driver Policy support page describes changes beginning with the April 2026 security update. On systems where that policy applies and is enforcing, kernel-driver acceptance depends on WHCP certification or an applicable legacy allow-list entry. The page distinguishes audit and enforcement states; do not assume every Windows 11 installation has identical enforcement. [Microsoft: Windows Driver Policy](https://support.microsoft.com/en-us/windows/hardware/drivers/the-windows-driver-policy)

Microsoft's signing-options documentation describes attestation as a testing route, without HLK certification, and excludes retail Windows Update publication for attestation-signed drivers. Therefore, a research prototype's signing route should not be assumed adequate for general distribution. Confirm the applicable submission route when a driver model and deployment audience are known. [Microsoft: Driver Signing Options](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings)

### Memory integrity and architecture

Memory integrity, also called HVCI, is enabled by default on compatible clean Windows 11 installations. Microsoft recommends code-integrity checks, functional testing with memory integrity enabled, and relevant HLK testing. A valid signature and correct runtime behavior are separate things to verify. [Microsoft: Driver compatibility with memory integrity and VBS](https://learn.microsoft.com/en-us/windows-hardware/test/hlk/testref/driver-compatibility-with-device-guard)

For each candidate, record Windows build/update level, x64 or ARM64, Secure Boot, memory integrity, and applicable application-control policy. Treat application architecture and driver-package architecture as separate compatibility questions. A vendor's x64 claim does not establish ARM64 support.

Disabling security protections should not be treated as proof of normal Windows 11 compatibility. Any development-only test configuration should be identified separately from the intended deployment configuration.

### Build and installation research

For a custom driver, choose a supported Visual Studio/SDK/WDK combination from Microsoft's current WDK instructions. The SDK and WDK build numbers must match. At the research date, the page recommends WDK 28000.2526 with Visual Studio 2026 and identifies WDK 26100.6584 for Visual Studio 2022 users. These are dated observations, not permanent project requirements. [Microsoft: Download the WDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)

Investigate stable device identity, COM-name allocation, administrator installation, standard-user access, upgrade rollback, and complete uninstall. Include service startup and recovery if the selected solution uses a service. No installer design is selected here.

## 4. Behavior that needs investigation before implementation

Windows serial compatibility extends beyond `ReadFile` and `WriteFile`. The documented API includes configuration, timeouts, modem status, event masks, break signaling, and purge operations. [Microsoft: Communications Functions](https://learn.microsoft.com/en-us/windows/win32/devio/communications-functions)

For example, `WaitCommEvent` supports receive, transmit, line-error, and modem-signal events, with defined overlapped-I/O behavior. A virtual endpoint that transfers bytes but mishandles these events may fail with clients that depend on them. [Microsoft: WaitCommEvent](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-waitcommevent)

The following are engineering questions and deductions, not claims that Windows or an examined product automatically resolves them:

| Area | What to investigate |
| --- | --- |
| Receive copies | A broadcast needs independent delivery state. One client's reads must not consume another client's copy. Determine what late subscribers receive. |
| Slow consumers | With finite buffers and a source that cannot be paused indefinitely, lossless delivery to an arbitrarily stalled client cannot be guaranteed. Compare dropping, disconnecting, and source backpressure. |
| Multiple writers | Serializing writes prevents simultaneous forwarding but does not identify protocol transactions or assign replies to requesters. Determine whether one writer is sufficient. |
| Framing | Do not assume one write call is one command, or one read completion is one response. Consult the actual device protocol and capture behavior. |
| Port configuration | Which client, if any, controls physical baud rate, parity, stop bits, and flow control? What happens when another requests conflicting settings? |
| Control lines | Decide how DTR, RTS, CTS, DSR, carrier detect, and break map to physical and virtual endpoints. Confirm whether line changes reset or otherwise affect the device. |
| Purging and cancellation | Determine whether a client's purge affects only its queue or the shared source. Closing one client must have a defined effect on pending operations and other clients. |
| Failures | Define observable behavior after USB removal, device reconnection, service failure, sleep/resume, and port renumbering. Avoid silently attaching to the wrong replacement device. |
| Timing | Measure latency, throughput, burst handling, queue growth, and timeout behavior against application needs. No performance numbers are established by this research. |
| Access | Determine who may observe traffic, transmit, and change configuration; restrict management and data endpoints accordingly. |

## 5. Possible next research steps

These steps gather evidence and can change the preferred direction. They are not a commitment to implement a particular architecture.

1. **Describe the real workload.** Identify the physical device, protocol, applications, client count, and whether each application reads, writes, or changes settings. Establish whether clients can select separate virtual ports.
2. **Record the Windows 11 target.** Capture architecture, edition/build, updates, security settings, and intended installation environment. Decide whether ARM64 is required.
3. **Build a compatibility checklist.** List serial APIs and control behavior required by the actual clients. Use API documentation and observation, rather than assuming full UART emulation is necessary.
4. **Evaluate an existing implementation in a test environment.** Start with receive duplication and one writer where applicable. Record exact package version, origin, signing status, configuration, and results. A failure should be classified as installation, API behavior, routing, protocol, or timing.
5. **Inspect the upstream examples.** Study hub4com's documented routing and Microsoft's UMDF 2 sample. Record which requirements are demonstrated and which remain unsupported or unknown. Pin exact source revisions before any later implementation work.
6. **Compare build versus reuse with evidence.** Consider integration control, compatibility gaps, licensing, deployment effort, maintenance, and support. Review licenses bundled with the exact artifacts before copying or distributing them.
7. **If custom development is justified, run a limited feasibility experiment.** First validate device discovery, open/close, required serial controls, cancellation, and installation. Expand to shared traffic only after those results are understood.

Suggested later test cases include two clients receiving identical numbered input; one client stalling; concurrent writer attempts; conflicting settings; closing during pending I/O; unplug/replug; reboot and sleep/resume; and uninstall/reinstall. Record byte counts, ordering, errors, and latency rather than reporting only that a terminal displayed text.

## 6. Conclusions currently supported by the evidence

Windows serial splitting is an established capability with existing implementations. com0com plus hub4com documents one practical arrangement, and VSPE documents a commercial splitter with Windows 11 support claims. Microsoft's UMDF 2 example provides another research starting point for custom virtual-device work.

The available evidence does **not** select a winner, validate any downloaded driver on the target machine, establish a performance budget, or prove that arbitrary multi-writer protocols can be shared transparently. The most consequential open questions are client behavior, write ownership, serial-control semantics, and compatibility with the exact Windows 11 deployment configuration.
