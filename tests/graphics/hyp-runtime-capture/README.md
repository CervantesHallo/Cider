# Real official launcher capture

The two PNGs are actual private-bottle HYP windows before and after 0031.
The official fixed 1.18.0.380 payload remains byte-identical. No original
account, bottle or game files were copied; these are runtime observations,
not installer, login or game acceptance.

`inventory.swift` returns only private-engine window metadata.
`capture.swift` uses a descriptor-pinned 64-byte parent request containing the
exact CGWindow/PID, actual kernel birth and title SHA256. It keeps the existing
ScreenCaptureKit permission preflight, no-follow paths, freshness, deadline,
exclusive PNG output and disk hash readback. It does not request permission or
control the interface. Fixed local lab paths are intentional evidence bounds.

Source, actual loaded renderer context, client/visible geometry, blank startup
observation and final visual result are kept separate in
[report 136](../../../docs/research/136-r-v1-real-hyp-caption-20261102.md).
