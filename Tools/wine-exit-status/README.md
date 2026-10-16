# Windows exit-status collector

Mac host process status is only an 8-bit Unix exit value; do not use it to infer
the complete Windows code, or map its low byte to MSI reboot success codes.

This tool starts one explicit executable with `CreateProcessW`, waits for its
process handle, and records its full `GetExitCodeProcess` DWORD. The isolated
installer implementation uses it before separate recipe detection. The App builder
compiles both PE architectures into its private resource stage, generates a
machine/digest manifest, then signs the enclosing App. It reuses Cider's existing
MinGW toolchain or host cross compilers on PATH; it does not download a compiler.
Executable helpers are never selected through CIDER_DATA or recipe URLs.

```text
cider-exit-status.exe RESULT.json TIMEOUT_MS CWD_OR_DASH EXE [ARGUMENT...]
```

Use absolute drive paths; `-` keeps the inherited cwd. Timeout accepts 1–3600000
milliseconds, followed by at most 5000 ms to observe termination of the created
primary process. The parent must budget for launch, that cleanup margin and I/O.
There is no independent deadline thread: this is not the Windows machine-time
watchdog. Do not use it to close a machine-time reservation.

The result must not exist. Existing reports are never replaced. Paths through
known reparse parents are rejected, but this is not a hostile-process sandbox or
race-proof path authority; future host integration must own/validate a private
result directory. No passwords, account names, arguments or environment values
are written into its result. No hooks, drivers, privilege changes or game edits.

Helper exit zero means the report is complete, including a terminated process's
exit code. The child may have failed: consumers must validate schema, `phase`,
`win32_error`, `child_created`, `primary_terminated` and `windows_exit_code`.
Failure, timeout, unavailable exit code or partial output is not success.
The helper doesn't collect descendant exit codes or stop/claim cleanup of child
handoffs; a primary exit must still be followed by the recipe's detection step.
`CiderCore.WindowsExitStatus` provides a bounded decoder and a guarded complete
code accessor. MSI reboot semantics apply to actual MSI installation or a trusted
recipe's explicit `exit_code_convention: msi` declaration. Other EXEs require zero;
nonzero DWORDs are retained in diagnostics and cannot be rescued by detected files.
Restart requests are returned to the GUI/CLI; the GUI does not auto-launch the
installed application in that outcome. It does not automatically restart a bottle.

Minimal Windows quoting preserves empty/escaped/Unicode arguments while leaving
ordinary switches unquoted. Wine msiexec's raw command-line parser retains quotes
on switches; the previous quote-everything helper could open its usage window
instead of honoring /qn. Real Wine collector and own EXE/MSI integration checks
are under tests/runtime/exit-status/results. Exception-valued ExitProcess probes
are not unhandled-fault injection or native Windows/kernel reference results.
Preserve launcher graphics environment, preflight and bottle-wide sync/locale.
Never retrofit Windows status by accepting its truncated Unix low byte.

Contracts: [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw),
[GetExitCodeProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess),
[MSI exit codes](https://learn.microsoft.com/en-us/windows/win32/msi/error-codes).
