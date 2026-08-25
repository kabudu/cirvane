# Prior art matrix

This matrix records the Stage 1 systematic comparison for the bounded recovery
transaction hypothesis. It is an internal diligence record, not a patent
clearance or a novelty proof. Search date: 2026-08-25. Claim wording remains
provisional.

The compared mechanism is narrow: a kernel-owned atomic recovery transition that
couples a service epoch, stale-work invalidation, bounded resource reclamation,
restart budget, capability-lease revocation, recoverable state generation and a
fixed-size inspectable outcome, under compile-time RAM and execution bounds,
without treating timeout or partial recovery as healthy state.

Scheduling, message passing, capabilities, watchdogs, restart supervision,
journalling, rollback, MPU/PMP isolation and event logs are prior art and are
not Cirvane inventions.

## Search log

Queries were run on 2026-08-25 across official RTOS documentation, GitHub,
project sites, Google Patents, and web search of papers and preprints. Negative
and close results are retained.

| Query | Venue | Material result |
|---|---|---|
| bounded recovery transaction MCU kernel | web, GitHub | No exact mechanism name; closest are Hubris restart, VirtuosoNext task recovery, Minix reincarnation |
| epoch reclamation kernel service restart | web, papers | EBR/RCU/hazard pointers are memory-reclamation schemes, not service recovery transactions |
| Hubris reinitialize_task generation lease | Hubris reference, cliffle | Closest production MCU OS: supervisor-task policy, generation, lease revocation |
| Tock process restart grant revoke | Tock design docs | Process restart and grant revocation; capsules are not atomic recovery transactions |
| Zephyr thread abort watchdog recovery | Zephyr docs | Application/supervisor recovery, not a kernel recovery transaction |
| seL4 Microkit fault endpoint restart | seL4/Microkit docs | Fault IPC to a root task; recovery policy is userspace |
| Minix 3 reincarnation server | Minix/research | Userspace reincarnation, not MCU kernel-owned |
| Erlang OTP supervisor restart | OTP docs | BEAM userspace supervision; cited as conceptual ancestor, out of MCU kernel scope |
| Theseus OS reconstitution | Theseus papers | Language-level state-spill reconstitution, not MCU recovery transactions |
| Composite OS recovery | Composite docs | Recov/fault isolation research OS, different hardware and model |
| S3K RISC-V PMP monitor recovery | kth-step/s3k | Monitor capabilities for partition recovery; not epoch-tagged message invalidation |
| VirtuosoNext fine-grain recovery | Altreonic | Kernel abort handler plus task reinit with optional blackboard restore |
| US12314734 microkernel service extensions | Google Patents | Isolation, escrow of capability spaces, reclamation against malicious processes |
| ESP-IDF Wi-Fi without FreeRTOS | ESP-IDF v6.0.2 docs, esp_adapter.c | Wi-Fi driver creates FreeRTOS tasks and uses queues/event groups |
| FreeRTOS task delete create watchdog | FreeRTOS docs | Baseline application-level supervision substrate |
| Apache NuttX task restart | NuttX docs | POSIX process/task model, not the Cirvane claim |
| RIOT OS thread restart | RIOT docs | IoT RTOS threads; no recovery transaction primitive |
| ZeroKernel watchdog recovery | ZeroBits GitHub | Cooperative orchestration, watchdog/safe mode, not kernel-owned transactions |

Conference programmes and preprint servers were searched via the same queries.
No paper was found that uses the exact Cirvane coupling under MCU RAM bounds.
That absence is not proof of worldwide novelty.

## System comparison

| System | Established mechanism | Trust boundary | Hardware assumptions | Resource model | Failure semantics | Evaluation | Overlap | Remaining difference to establish |
|---|---|---|---|---|---|---|---|---|
| FreeRTOS | Tasks, queues, timers, task delete/create, hook/watchdog recovery | Application supervisor over a general scheduler | MCU with optional MPU | Dynamic or static TCB/queue objects | Task restart is policy in the application; stale queue items can remain | Matched Cirvane baseline already recorded | Restart, queues, static allocation option | Kernel-owned atomic epoch invalidation plus evidence; no hidden FreeRTOS |
| ESP-IDF | Boot, drivers, Wi-Fi, OTA, event loop | Vendor HAL plus FreeRTOS | ESP32-C5 ROM/HAL | Heap plus static | Driver tasks assume FreeRTOS | Current firmware substrate | Flash, USB, radio binaries | Cirvane must own scheduling; radio remains a feasibility gate |
| Zephyr | Priority scheduling, userspace, memory domains, thread abort, watchdog | Kernel plus optional user mode | MPU/MMUs on supported SoCs | Configurable kernel objects | Recovery is thread/workqueue/watchdog policy | Broad RTOS benchmarks | Isolation and restart building blocks | Atomic recovery transaction with stale-work prohibition |
| Apache NuttX | POSIX tasks, signals, filesystems | Monolithic POSIX RTOS | Broad MCU/MMU set | POSIX process/heap model | Process restart is POSIX, not Cirvane RTX | POSIX suites | General embedded OS category | Cirvane is not a POSIX competitor |
| Tock | Rust kernel, process isolation, grants, restartable processes | Kernel/capsules/processes | MPU, no dynamic kernel alloc in grants path | Capsules + processes | Grant revoke on restart; not one recovery transaction coupling epoch, budget and evidence | Tock test/process model | Isolation, restart, grant revoke | Distinguish atomic bounded recovery from language/MPU isolation |
| RIOT | Tick scheduler, threads, networking | Broad IoT RTOS | Constrained MCUs | Thread stacks | Thread restart is application policy | IoT OS comparisons | Constrained-device OS | One-board Cirvane scope and kernel RTX |
| seL4 | Capability microkernel, formal verification, fault endpoints | Minimal kernel; policy in user tasks | MMU, verified configs | Capabilities, paging | Faults become IPC; recovery is userspace | Formal proofs, WCET on supported platforms | Capabilities, fail isolation | Cirvane must not claim verification; RTX is kernel-owned on MCU PMP |
| seL4 Microkit | Static user PDs, fault IRQs to a root PD | Root task policy | seL4 hardware class | Static system description | Restart policy lives in the root PD | Microkit examples | Static tasks, fault notification | Kernel-owned transaction vs root-task supervisor |
| Hubris | Static tasks, in-place reinit, generation, leases revoked when sender resumes | Kernel provides reinit; **supervisor task owns policy** | MPU, no dynamic kernel alloc | Fixed tasks, message+lease | Crash notifies supervisor; stale leases revoked; RAM not cleared by kernel | Oxide production firmware, Humility | Closest MCU OS: generation, revoke, static tasks, in-place restart | Cirvane hypothesizes kernel-owned atomic coupling of epoch, reclaim, budget, fail-closed health and fixed evidence rather than a supervisor-task policy |
| Minix 3 | Reincarnation server restarts drivers | Userspace server | MMU, POSIX | Processes | Driver reincarnation after crash | Research and historical OS | Restartable services | Userspace reincarnation vs MCU kernel primitive |
| Erlang/OTP | Supervisor trees, restart intensity | BEAM VM | Not MCU kernels | Process heaps | Let-it-crash with bounded restart | Decades of telecom practice | Conceptual supervision ancestry | Out of MCU kernel claim scope |
| Theseus | State-spill reconstitution of components | Language/runtime | x86 research kernel | Rust components | Reconstitute from spilled state | Research prototypes | Recoverable components | Different hardware and mechanism |
| VirtuosoNext | Fine-grain partitioned task abort and reinit | Kernel abort handler | ARM Cortex-M MPU | Partitioned tasks | Abort, optional blackboard restore, measured tens of microseconds | Vendor measurements | Kernel-involved task recovery | No epoch-tagged message invalidation or fixed recovery evidence record as specified |
| S3K | RISC-V PMP separation kernel, monitor capabilities | Trusted monitor process | RISC-V PMP | Time/memory capabilities | Monitor supervises and reconfigures partitions | Academic prototype | PMP, monitor recovery | Partition monitor vs service-epoch transaction |
| ZeroKernel | Cooperative scheduler, watchdog, safe mode, capability masks | Library runtime, not a privileged kernel | MCU without requiring MPU | Fixed queues | Watchdog to recovery/safe mode | Project docs | Bounds, watchdog, caps | Not a privileged kernel recovery transaction |
| US12314734B1 | Microkernel service isolation, capability escrow, reclamation against malicious processes | OS service extensions over seL4-like kernels | Capability microkernels | Process-owned resources | Isolation enables recovery; escrow prevents unreclaimable capability graphs | Patent disclosure | Reclamation and isolation | Patent is isolation/escrow, not Cirvane's MCU recovery transaction |

## Closest systems and remaining hypothesis

Hubris is the closest reviewed MCU operating system. It already provides static
tasks, in-place reinitialization, generation numbers, and atomic lease
revocation when a sender resumes. Recovery **policy** is deliberately outside
the kernel, in a supervisor task.

VirtuosoNext already performs kernel-assisted task abort and reinit with
optional saved state. Minix 3 and Erlang already demonstrate restartable
services at other layers. seL4/Microkit already route faults to a policy task.

NOV-01 therefore survives Stage 1 only as a **narrow, provisional hypothesis**:
that making the recovery transition a single kernel-owned atomic object, with
forbidden healthy collapse during partial recovery and a fixed-size evidence
record, is not already present in the reviewed MCU kernels at this coupling.
A later matched prototype may falsify that difference as a composition of
Hubris-style generation plus supervisor restart. Stage 1 does not claim it is
novel.

## Non-results retained

- No reviewed source used the phrase "bounded recovery transaction" as a kernel
  primitive with this coupling.
- Epoch-based reclamation literature (EBR, RCU) addresses safe memory reuse,
  not service-epoch invalidation of in-flight messages.
- ESP-IDF documents FreeRTOS as the system component; Wi-Fi initialisation
  creates a driver task. That is a platform dependency finding, not prior art
  for the recovery mechanism.

This matrix bounds current claims. Independent novelty challenge and clean-room
reproduction remain optional post-release assurance and are not satisfied here.
