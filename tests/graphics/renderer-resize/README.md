# Actual window and backbuffer growth

This extends renderer-owner-transition without replacing its device/context or
two swapchains. Initial buffers are 160×100; on the foreign root, real child
windows grow from 180×100 to 220×120 and the two buffers become 200×120 through
`ResizeBuffers`. Both roots' actual client sizes are checked as they become
540×300. The renderer returns to its own root with the enlarged resources.

Each frame reads every actual mapped pixel: 16,000 per target initially and
24,000 in each later phase, 128,000 total. Six old backbuffer/RTV/staging owners
are released before resizing; fourteen final releases give twenty total. These
are actual COM calls, not claims that every internal reference reached zero.

The external oracle also requires actual colored rectangles to grow from
356×196 to 436×236 pixels and their relative x-distance from 400 to 520. Merely
passing GPU readback cannot satisfy that condition. `before-fix-phase2.png`
shows the real old-size result; the other six PNGs are final real captures.

Use matched 0028–0030. 0030 already contains the normal TRACE observations;
do not apply the older reparent diagnostic patch on top. `capture.swift`
retains the exact qualified private paths and permits only the defined token
dimensions for each phase. `geometry.py` reads actual completed message
callbacks, distinguishing them from callback tails interrupted by process exit.

Use the same two MinGW build commands and isolated GUI/XPC spawner as the
earlier consumer. Source review, exact artifacts, failures, callback evidence
and limits are recorded in [report 135](../../../docs/research/135-r-v1-exported-resize-20261102.md).
