# Windows CLI task runner

Implements the process limit in [plan/14](../../docs/plan/14-windows-test-access.md).
It is prepared locally; native Windows verification and deployment remain pending.
It does not load drivers, change boot protection, install services, or connect to a host.

## Build

In the existing x64 EWDK/VS2022 environment, run `build.cmd`. The two ordinary
console programs are written to `out/windows-task-runner/` in the repository root.
The fixture only prints arguments, exits, sleeps, creates its requested STOP file,
or creates another copy of itself. It has no account, network, driver, or VM operations.
An optional `/p:DeadlineFixture=true` build of `runner.vcxproj` deliberately blocks
its main thread after starting the synthetic child. This separate binary exercises
the independent deadline guard; it is never enabled by a production runtime option.

## Invocation

```text
cider-task-runner.exe UUID SECONDS RECEIPT STOP_FILE ABSOLUTE_EXE [ARG ...]
```

- UUID is the lowercase reservation ID already charged by the Mac budget ledger.
- SECONDS is 10–600 and includes a five second cleanup reserve; it is not a fresh budget.
  The controller must subtract SSH/transfer/startup time already spent and reserve
  final publication/return time before choosing this limit. Do not restart the full
  reservation clock for each remote command; refuse launch if less than ten seconds remain.
- The full reservation must fit within 09:00–17:00 UTC+8. The native runner also
  stops when that window closes, its elapsed deadline arrives, or STOP_FILE appears.
- Receipt and STOP_FILE use existing directories on fixed local disks. EXE may
  additionally use a mounted read-only ISO, for the existing EWDK. Network/device
  paths, alternate streams and observed reparse points are rejected.
- Use a fresh receipt path for each reservation. Existing receipts/partial files
  are refused. The final JSON is published by rename after flushing its partial file.
- EXE is launched directly with separately quoted Windows CRT arguments. No shell
  is inserted. A shell explicitly selected as EXE has its own argument syntax;
  caller-controlled scripts must not concatenate untrusted shell text.

## Ownership and evidence

The child is created suspended with `PROC_THREAD_ATTRIBUTE_JOB_LIST`, verified in
the unnamed job, and then resumed. There is no post-creation assignment fallback.
The job enables `KILL_ON_JOB_CLOSE` and does not allow breakaway. Only duplicated
stdout/stderr and NUL stdin handles are inherited, excluding the job and receipt.
Ordinary `CreateProcess` descendants remain tracked even after the primary exits.
Cleanup requires a successful active-process query showing zero; failed queries,
termination or exhausted cleanup time cannot be promoted to confirmed cleanup.
An independent deadline thread also terminates the job and runner at the full
reservation deadline if the main thread stalls. It cannot invent a final cleanup
receipt on that path: the partial file remains unconfirmed and the controller must
recover real process state before closing its reservation.

Receipts record the primary exit code separately from the runner result. A nonzero
primary exit, timeout, STOP, setup failure, publication failure or unconfirmed cleanup
returns failure. A clean job is not evidence that the requested contract passed.
`elapsed_seconds` measures through child cleanup, before receipt publication;
the transport must also measure the full remote operation and check runner completion.
Do not use the receipt alone to certify SSH teardown or the full machine occupation.

This owns ordinary CLI processes, not arbitrary activities started through WMI,
services, scheduled tasks, an existing VM manager or a separate RDP session.
It is not a malicious-process sandbox or an atomic filesystem boundary against
another process replacing directories. Native job nesting, inherited handles, forced
runner exit, STOP, the time window and receipt failures must be verified on Windows
before enabling unattended execution. Cumulative budget and duplicate dispatch
still require the persistent controller; this executable cannot authorize itself.

## Sources

- [Windows job ownership and close behavior](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
- [Microsoft job-list and handle-list creation attributes](https://github.com/MicrosoftDocs/sdk-api/blob/docs/sdk-api-src/content/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute.md)
- [Job process accounting](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information)
