# Actual foreign-window renderer migration

This extends [renderer-consumer](../renderer-consumer/) using ordinary Win32,
D3D11 and DXGI calls. The host owns two top-level windows. The renderer retains
one device, one immediate context and the same two swapchains while moving both
child windows Root1 → Root2 → Root1 with `SetParent`.

Three real image gates require red/blue, blue/red, then red/red. The last image
differs from both earlier phases. Each frame still checks all 16,000 mapped
pixels. Nonblocking `Map` retries only `DXGI_ERROR_WAS_STILL_DRAWING`, within the
existing five-second GPU budget and global deadlines; other errors fail. A
successful Map is owned before any post-call clock check so late results still
get `Unmap` during cleanup.

`diagnostic.patch` adds read-only observations to 0028. Before captures 2/3,
the controller observes actual same-context route migration and the old
routes' real read-only retirement permission. Before gate 4 it requires all six
exact `(child, context, root, lease, ticket)` routes retired and both local
renderer owners released. The process remains alive until the final ACK; the
host then joins it with exit code zero.

`oracle.py` checks the retained Win32 records. `capture.swift` and `analyze.py`
are the exact qualified capture/analyzer sources, with fixed private lab paths
retained as evidence constraints. They do not request screen permissions.
Six included PNGs are actual WindowServer captures. Build `client.c` with the
same two MinGW commands as renderer-consumer, and launch only through the
isolated packaged Cider GUI/XPC transport.

This does not cover initial local-to-foreign conversion, resizing, CEF, input,
performance or game compatibility. A local retired/freed observation is not
completion of every asynchronous Cocoa tail or global GPU activity. See
[report 132](../../../docs/research/132-r-v1-renderer-reparent-20261102.md).
