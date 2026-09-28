#!/usr/bin/env swift
//
// Generates the Cider app icon.
//
//   scripts/make-icon.swift            # -> App/Resources/AppIcon.icns (+ icon-1024.png, out/icon-preview.png)
//
// Every slice is redrawn from the vector definition at its native pixel size rather than
// downscaled from 1024, and the small slices drop the fine detail (specular, glass sheen) and
// thicken the window frame so the mark still reads at 16pt.
//
// Mark: a cider-amber squircle carrying a cream apple whose core is a four-pane sash window —
// Windows programs inside an Apple machine. Palette matches App/Sources/Cider/Theme.swift.

import AppKit
import ImageIO
import UniformTypeIdentifiers

// MARK: - Palette

func rgb(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(srgbRed: CGFloat((hex >> 16) & 0xFF) / 255,
            green: CGFloat((hex >> 8) & 0xFF) / 255,
            blue: CGFloat(hex & 0xFF) / 255,
            alpha: alpha)
}

let tileTop = rgb(0xFFD47F)   // sunlit cider
let tileMid = rgb(0xE8A33D)   // Theme.accent
let tileLow = rgb(0xC9691F)
let tileBot = rgb(0xAE5119)   // pressed-apple bottom
let cream = rgb(0xFDF7EA)
let creamEdge = rgb(0xD9C49D)
let leafFill = rgb(0x6E8F3C)
let stemFill = rgb(0x7A3F14)

// MARK: - Geometry

/// Apple's icon corner: a superellipse (|x|^n + |y|^n = 1), n = 5.
func squircle(in r: CGRect, n: Double = 5) -> CGPath {
    let path = CGMutablePath()
    let a = r.width / 2, b = r.height / 2
    let steps = 1440
    for i in 0...steps {
        let t = Double(i) / Double(steps) * 2 * .pi
        let ct = cos(t), st = sin(t)
        let x = pow(abs(ct), 2 / n) * (ct < 0 ? -1 : 1)
        let y = pow(abs(st), 2 / n) * (st < 0 ? -1 : 1)
        let p = CGPoint(x: r.midX + a * CGFloat(x), y: r.midY + b * CGFloat(y))
        i == 0 ? path.move(to: p) : path.addLine(to: p)
    }
    path.closeSubpath()
    return path
}

/// Apple body in a normalised box, x and y in [-1, 1], y up. No bite: this is a cider apple.
/// Deep dip between the shoulders and a shallow dip under the base are what make it read as an
/// apple rather than a blob at 32 pt.
func applePath() -> CGPath {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: 0, y: 0.46))
    p.addCurve(to: CGPoint(x: 0.56, y: 0.93), control1: CGPoint(x: 0.09, y: 0.74), control2: CGPoint(x: 0.29, y: 0.93))
    p.addCurve(to: CGPoint(x: 1.00, y: 0.14), control1: CGPoint(x: 0.86, y: 0.93), control2: CGPoint(x: 1.00, y: 0.58))
    p.addCurve(to: CGPoint(x: 0.71, y: -0.71), control1: CGPoint(x: 1.00, y: -0.26), control2: CGPoint(x: 0.95, y: -0.54))
    p.addCurve(to: CGPoint(x: 0.26, y: -0.97), control1: CGPoint(x: 0.56, y: -0.88), control2: CGPoint(x: 0.42, y: -0.97))
    p.addCurve(to: CGPoint(x: 0, y: -0.93), control1: CGPoint(x: 0.16, y: -0.98), control2: CGPoint(x: 0.07, y: -0.95))
    p.addCurve(to: CGPoint(x: -0.26, y: -0.97), control1: CGPoint(x: -0.07, y: -0.95), control2: CGPoint(x: -0.16, y: -0.98))
    p.addCurve(to: CGPoint(x: -0.71, y: -0.71), control1: CGPoint(x: -0.42, y: -0.97), control2: CGPoint(x: -0.56, y: -0.88))
    p.addCurve(to: CGPoint(x: -1.00, y: 0.14), control1: CGPoint(x: -0.95, y: -0.54), control2: CGPoint(x: -1.00, y: -0.26))
    p.addCurve(to: CGPoint(x: -0.56, y: 0.93), control1: CGPoint(x: -1.00, y: 0.58), control2: CGPoint(x: -0.86, y: 0.93))
    p.addCurve(to: CGPoint(x: 0, y: 0.46), control1: CGPoint(x: -0.29, y: 0.93), control2: CGPoint(x: -0.09, y: 0.74))
    p.closeSubpath()
    return p
}

/// Stem rising out of the dip; stroked, not filled.
func stemPath() -> CGPath {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: 0.01, y: 0.52))
    p.addCurve(to: CGPoint(x: 0.13, y: 1.02), control1: CGPoint(x: 0.03, y: 0.74), control2: CGPoint(x: 0.06, y: 0.90))
    return p
}

/// Leaf off the stem, same normalised space as `applePath`.
func leafPath() -> CGPath {
    let p = CGMutablePath()
    p.move(to: CGPoint(x: 0.09, y: 0.74))
    p.addCurve(to: CGPoint(x: 0.86, y: 0.98), control1: CGPoint(x: 0.30, y: 0.98), control2: CGPoint(x: 0.62, y: 1.06))
    p.addCurve(to: CGPoint(x: 0.09, y: 0.74), control1: CGPoint(x: 0.66, y: 0.80), control2: CGPoint(x: 0.36, y: 0.66))
    p.closeSubpath()
    return p
}

/// The four sash panes, in the same normalised space. The window fills most of the apple so the
/// mark reads as a window at a glance and as an apple in silhouette; `bold` thickens the frame
/// for the small slices, where a hairline mullion would disappear.
func panes(bold: Bool) -> [CGRect] {
    let box = CGRect(x: -0.67, y: -0.80, width: 1.34, height: 1.20)
    let border: CGFloat = bold ? 0.15 : 0.11
    let bar: CGFloat = bold ? 0.14 : 0.10
    let inner = box.insetBy(dx: border, dy: border)
    let colW = (inner.width - bar) / 2
    let topH = (inner.height - bar) / 2         // near-square panes survive the small slices
    let botH = topH
    let xs = [inner.minX, inner.maxX - colW]
    return xs.flatMap { x in
        [CGRect(x: x, y: inner.maxY - topH, width: colW, height: topH),
         CGRect(x: x, y: inner.minY, width: colW, height: botH)]
    }
}

// MARK: - Drawing

func drawIcon(size: Int) -> CGImage {
    let s = CGFloat(size)
    let detail = size >= 128          // specular, glass sheen, rim light
    let bold = size < 64              // thicker window frame so it survives at 16/32 pt
    let space = CGColorSpace(name: CGColorSpace.sRGB)!
    let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8,
                        bytesPerRow: 0, space: space,
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    ctx.setAllowsAntialiasing(true)
    ctx.interpolationQuality = .high

    // macOS icon grid: the body is 824/1024 of the canvas, leaving room for the baked shadow.
    let inset = s * 100.0 / 1024.0
    let body = CGRect(x: inset, y: inset, width: s - 2 * inset, height: s - 2 * inset)
    let tile = squircle(in: body)

    // Baked drop shadow under the tile.
    ctx.saveGState()
    ctx.setShadow(offset: CGSize(width: 0, height: -s * 0.014), blur: s * 0.035,
                  color: rgb(0x2B1405, 0.34))
    ctx.addPath(tile)
    ctx.setFillColor(tileMid)
    ctx.fillPath()
    ctx.restoreGState()

    // Tile gradient.
    ctx.saveGState()
    ctx.addPath(tile)
    ctx.clip()
    let grad = CGGradient(colorsSpace: space,
                          colors: [tileTop, tileMid, tileLow, tileBot] as CFArray,
                          locations: [0, 0.42, 0.78, 1])!
    ctx.drawLinearGradient(grad, start: CGPoint(x: body.midX, y: body.maxY),
                           end: CGPoint(x: body.midX, y: body.minY), options: [])

    if detail {
        // Soft specular in the upper left, the way a glass of cider catches window light.
        let spec = CGGradient(colorsSpace: space,
                              colors: [rgb(0xFFFFFF, 0.18), rgb(0xFFFFFF, 0)] as CFArray,
                              locations: [0, 1])!
        ctx.drawRadialGradient(spec,
                               startCenter: CGPoint(x: body.minX + body.width * 0.28,
                                                    y: body.maxY - body.height * 0.18),
                               startRadius: 0,
                               endCenter: CGPoint(x: body.minX + body.width * 0.28,
                                                  y: body.maxY - body.height * 0.18),
                               endRadius: body.width * 0.62, options: [])
        // Inner rim: light along the top edge, shadow along the bottom.
        ctx.setLineWidth(max(1, s * 0.004))
        ctx.addPath(squircle(in: body.insetBy(dx: s * 0.002, dy: s * 0.002)))
        ctx.replacePathWithStrokedPath()
        ctx.clip()
        let rim = CGGradient(colorsSpace: space,
                             colors: [rgb(0xFFFFFF, 0.55), rgb(0xFFFFFF, 0.0),
                                      rgb(0x000000, 0.18)] as CFArray,
                             locations: [0, 0.45, 1])!
        ctx.drawLinearGradient(rim, start: CGPoint(x: body.midX, y: body.maxY),
                               end: CGPoint(x: body.midX, y: body.minY), options: [])
    }
    ctx.restoreGState()

    // Mark transform: normalised apple space -> tile, sized to ~62% of the body, nudged up for the leaf.
    let scale = body.width * 0.66 / 2
    let cx = body.midX
    let cy = body.midY - body.height * 0.035
    var xf = CGAffineTransform(translationX: cx, y: cy).scaledBy(x: scale, y: scale)

    // Apple with the panes knocked out (even-odd), so the cider shows through the glass.
    let mark = CGMutablePath()
    mark.addPath(applePath(), transform: xf)
    let radius = (bold ? 0.045 : 0.032) * scale
    for pane in panes(bold: bold) {
        let r = pane.applying(xf)
        mark.addPath(CGPath(roundedRect: r, cornerWidth: radius, cornerHeight: radius,
                            transform: nil))
    }

    ctx.saveGState()
    if detail {
        ctx.setShadow(offset: CGSize(width: 0, height: -s * 0.008), blur: s * 0.022,
                      color: rgb(0x5A2408, 0.30))
    }
    ctx.addPath(mark)
    ctx.setFillColor(cream)
    ctx.fillPath(using: .evenOdd)
    ctx.restoreGState()

    if detail {
        // Glass sheen across the panes only.
        ctx.saveGState()
        let glass = CGMutablePath()
        for pane in panes(bold: bold) {
            let r = pane.applying(xf)
            glass.addPath(CGPath(roundedRect: r, cornerWidth: radius, cornerHeight: radius,
                                 transform: nil))
        }
        ctx.addPath(glass)
        ctx.clip()
        let sheen = CGGradient(colorsSpace: space,
                               colors: [rgb(0xFFFFFF, 0.30), rgb(0xFFFFFF, 0.02),
                                        rgb(0x7A3410, 0.14)] as CFArray,
                               locations: [0, 0.55, 1])!
        let gb = glass.boundingBox
        ctx.drawLinearGradient(sheen, start: CGPoint(x: gb.minX, y: gb.maxY),
                               end: CGPoint(x: gb.maxX, y: gb.minY), options: [])
        ctx.restoreGState()
    }

    // Stem and leaf sit above the dip, in their own colours so they read against both the cream
    // body and the amber tile without needing a knockout gap.
    ctx.saveGState()
    ctx.addPath(stemPath().copy(using: &xf)!)
    ctx.setStrokeColor(stemFill)
    ctx.setLineWidth(max(1.5, scale * (bold ? 0.10 : 0.075)))
    ctx.setLineCap(.round)
    ctx.strokePath()

    let leaf = CGMutablePath()
    leaf.addPath(leafPath(), transform: xf)
    ctx.addPath(leaf)
    ctx.setFillColor(leafFill)
    ctx.fillPath()
    if detail {
        ctx.addPath(leaf)
        ctx.setStrokeColor(rgb(0x4F6B28, 0.7))
        ctx.setLineWidth(max(1, s * 0.003))
        ctx.strokePath()
    }
    ctx.restoreGState()

    return ctx.makeImage()!
}

// MARK: - Output

func writePNG(_ image: CGImage, to url: URL) {
    guard let dst = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString,
                                                    1, nil) else {
        fatalError("cannot write \(url.path)")
    }
    CGImageDestinationAddImage(dst, image, nil)
    guard CGImageDestinationFinalize(dst) else { fatalError("cannot finalize \(url.path)") }
}

let root = URL(fileURLWithPath: CommandLine.arguments.count > 1 ? CommandLine.arguments[1]
                                                                : FileManager.default.currentDirectoryPath)
let resources = root.appendingPathComponent("App/Resources")
let out = root.appendingPathComponent("out")
let iconset = out.appendingPathComponent("Cider.iconset")
for dir in [resources, out, iconset] {
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
}

// Native render per pixel size; .icns slices reuse them.
let sizes = [16, 32, 64, 128, 256, 512, 1024]
var rendered: [Int: CGImage] = [:]
for size in sizes { rendered[size] = drawIcon(size: size) }

let slices: [(String, Int)] = [
    ("icon_16x16", 16), ("icon_16x16@2x", 32),
    ("icon_32x32", 32), ("icon_32x32@2x", 64),
    ("icon_128x128", 128), ("icon_128x128@2x", 256),
    ("icon_256x256", 256), ("icon_256x256@2x", 512),
    ("icon_512x512", 512), ("icon_512x512@2x", 1024),
]
for (name, size) in slices {
    writePNG(rendered[size]!, to: iconset.appendingPathComponent("\(name).png"))
}
writePNG(rendered[1024]!, to: resources.appendingPathComponent("icon-1024.png"))

// Contact sheet: the real pixel sizes over light, mid and dark, to judge small-size legibility.
do {
    let space = CGColorSpace(name: CGColorSpace.sRGB)!
    let row = [512, 256, 128, 64, 32, 16]
    let pad = 24, w = 512 + pad * (row.count + 1) + row.dropFirst().reduce(0, +)
    let bands: [(CGColor, Int)] = [(rgb(0xF2F0EC), 0), (rgb(0x8A8A8A), 1), (rgb(0x1A1816), 2)]
    let bandH = 512 + pad * 2
    let ctx = CGContext(data: nil, width: w, height: bandH * 3, bitsPerComponent: 8,
                        bytesPerRow: 0, space: space,
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    for (color, i) in bands {
        let y = bandH * (2 - i)
        ctx.setFillColor(color)
        ctx.fill(CGRect(x: 0, y: y, width: w, height: bandH))
        var x = pad
        for size in row {
            ctx.draw(rendered[size]!, in: CGRect(x: x, y: y + pad, width: size, height: size))
            x += size + pad
        }
    }
    writePNG(ctx.makeImage()!, to: out.appendingPathComponent("icon-preview.png"))
}

print("iconset: \(iconset.path)")
print("preview: \(out.appendingPathComponent("icon-preview.png").path)")
