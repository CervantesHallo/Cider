# Renderer-owned local → foreign → local windows

The renderer owns Root1 and both child windows. The controller owns only Root2.
An initial ordinary 80-byte pipe bootstrap communicates the newly created
handles; the controller checks actual Win32 owners, parents and process births
before echoing it. The renderer creates D3D11 objects only after that echo.

The same device/context/two swapchains draw red/blue on local Root1, blue/red
after ordinary `SetParent` to foreign Root2, and red/red back on Root1. Three
actual screenshots and 16,000 real CPU pixel reads per frame are required.
Both owning window classes explicitly use a borrowed stock white brush, so the
white-gap oracle does not depend on an undefined or themed window background.

After GPU drain/COM release, the renderer destroys its own children and stays
alive with Root1 retained. The last gate requires all six actual routes retired
and both local renderer owners released. After ACK it destroys Root1 and exits;
the controller joins that real process, then destroys its own Root2. It never
tries to destroy another process's windows. Five messages in each direction
include bootstrap plus the four render/release phases.

Use matched 0028+0029 and `diagnostic.patch`; the latter adds only the existing
read-only route observations. `client.c`, `oracle.py`, capture/analyzer sources
and six real PNGs are frozen from report 134. Capture retains explicit private
lab paths and the user's existing test-window authorization. Build the client
with the same two MinGW commands as renderer-consumer and launch through the
isolated packaged Cider GUI/XPC spawner.

No default, resize, CEF, performance, physical-display or game acceptance is
implied. Local route retirement is not all asynchronous Cocoa tails or global
GPU idle. See [report 134](../../../docs/research/134-r-v1-owner-transition-20261102.md).
