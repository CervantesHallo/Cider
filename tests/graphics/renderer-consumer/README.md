# Standard D3D11 foreign-window consumer

`client.c` uses ordinary Win32, D3D11 and DXGI APIs. The host owns two child
windows; an opposite-bitness renderer creates two swapchains for those windows
on one device. It neither calls diagnostic Wine exports nor asks the host to
draw the images.

The first phase draws red/blue, the second blue/red. Each texture is copied to
a staging resource, mapped for CPU reading and checked at all 16,000 pixels.
The host publishes a 64-byte token and waits for the external image gate before
echoing the renderer's report. After GPU drain and COM release, the host destroys
the child windows while the renderer stays alive. The last gate requires the
actual Native resource owners to report `context0 routes0`; only then may the
renderer exit and the host join it.

Both architectures build with GCC, for example:

```sh
x86_64-w64-mingw32-gcc -std=gnu17 -O2 -g -Wall -Wextra -Werror \
  client.c -o renderer-consumer.exe -ld3d11 -ldxgi -luuid -luser32 -lgdi32
```

The experimental driver requires `WINEMAC_EXPERIMENTAL_REMOTE_RENDERER=1` and
the DXMT cross-process setting. Native and both PE drivers must have the same
eight-entry Unix layout. Run through the packaged Cider GUI/XPC spawner in an
isolated bottle; never launch a logged-in Steam bottle copy.

`capture.swift` is the exact qualified ScreenCaptureKit helper. Its fixed local
lab roots are intentional evidence constraints, not a portable product API.
The user explicitly authorized this capture method. It matches the nonce title
and an actual executable inside the specified private engine directory, checks
the pinned ready token again, and writes the captured PNG exclusively.
`analyze.py` verifies complete, separate colored rectangles and a white gap;
`oracle.py` checks the raw protocol and actual process exit. The four PNGs are
the real WindowServer captures from the final two directions, not mockups.

An earlier reverse run failed an extra `VirtualQuery` metadata check on a Metal
mapped address. That failure is retained. The final fixture follows successful
D3D11 `Map(READ)` with bounded actual pixel loads and `Unmap`; it does not claim
that Wine's mapping metadata was repaired. Source references and exact scope are
in [report 131](../../../docs/research/131-r-v1-renderer-consumer-20261102.md).
