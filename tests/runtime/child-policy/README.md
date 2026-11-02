# Engine child-policy admission

This benign executable is both the parent and the child. Run only through the
packaged GUI/XPC in an owned prefix without accounts or games. The controller copies each PE build
under `C:\CiderPreviewPolicy\pe32` or `pe64` as `allowed.exe` and the four image
names in `Preflight.gatedExecutables`; no vendor executable is used.

Build both architectures with the recorded mingw GCC, `-municode -std=gnu17
-O2 -Wall -Wextra -Werror`. Each parent runs against both child architectures.
The allowed child must actually finish with code 73. Each named denied child
must fail creation with Win32 error 1260, including the mixed-case name.
Retain raw logs, session environment, exact loader/native module hashes and
actual cleanup receipts before admitting an engine policy binding. This is
child-launch policy evidence, not a game or full-engine qualification.
