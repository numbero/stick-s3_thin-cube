import AppKit
import CoreGraphics
import Foundation

// Historical ThinCube v4 generator. The selected v6 application assets are
// installed by scripts/build-icons.sh; this file is retained for v4 reproduction.
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let appOnly = CommandLine.arguments.contains("--app-only")
let appStyle = CommandLine.arguments.firstIndex(of: "--style").map { CommandLine.arguments[$0 + 1] } ?? "negative"
let styles = ["negative", "cobalt", "outline"]
precondition(styles.contains(appStyle), "Unknown application icon style")
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)

func color(_ hex: UInt32) -> CGColor {
    CGColor(red: CGFloat((hex >> 16) & 255) / 255,
            green: CGFloat((hex >> 8) & 255) / 255,
            blue: CGFloat(hex & 255) / 255, alpha: 1)
}

func rounded(_ context: CGContext, _ rect: CGRect, _ radius: CGFloat, _ fill: CGColor) {
    context.setFillColor(fill)
    context.addPath(CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius, transform: nil))
    context.fillPath()
}

func drawAppStyle(in context: CGContext, style: String) {
    context.saveGState()
    context.translateBy(x: 0, y: 1024)
    context.scaleBy(x: 1, y: -1)
    let tile: UInt32 = style == "negative" ? 0x101820 : (style == "cobalt" ? 0x2563EB : 0xF0F3F7)
    let body: UInt32 = style == "negative" ? 0xE8EFF5 : 0x17212E
    let screen: UInt32 = style == "negative" ? tile : (style == "cobalt" ? 0xF7FAFF : 0xDDE8F3)
    let button: UInt32 = style == "cobalt" ? 0x55B6FF : 0x3478F6
    rounded(context, CGRect(x: 80, y: 80, width: 864, height: 864), 190, color(tile))
    let enclosure = CGRect(x: 336, y: 176, width: 352, height: 672)
    if style == "outline" {
        context.setStrokeColor(color(body))
        context.setLineWidth(22)
        context.addPath(CGPath(roundedRect: enclosure.insetBy(dx: 11, dy: 11), cornerWidth: 53, cornerHeight: 53, transform: nil))
        context.strokePath()
    } else {
        rounded(context, enclosure, 64, color(body))
    }
    rounded(context, CGRect(x: 384, y: 220, width: 256, height: 456), 24, color(screen))
    rounded(context, CGRect(x: 424, y: 744, width: 176, height: 28), 14, color(button))
    context.restoreGState()
}

func drawApp(in context: CGContext) { drawAppStyle(in: context, style: appStyle) }

func drawMenu(in context: CGContext, color: CGColor = CGColor(gray: 0, alpha: 1)) {
    context.saveGState()
    context.translateBy(x: 0, y: 18)
    context.scaleBy(x: 1, y: -1)
    context.setShouldAntialias(false)
    let path = CGMutablePath()
    path.addRoundedRect(in: CGRect(x: 5, y: 1, width: 8, height: 16), cornerWidth: 1.5, cornerHeight: 1.5)
    path.addRect(CGRect(x: 6, y: 3, width: 6, height: 9))
    path.addRect(CGRect(x: 7, y: 14, width: 4, height: 1))
    context.setFillColor(color)
    context.addPath(path)
    context.drawPath(using: .eoFill)
    context.restoreGState()
}

func png(_ filename: String, pixels: Int, logicalSize: CGFloat, draw: (CGContext) -> Void) throws {
    guard let context = CGContext(data: nil, width: pixels, height: pixels,
                                  bitsPerComponent: 8, bytesPerRow: pixels * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else {
        fatalError("Cannot allocate bitmap")
    }
    context.scaleBy(x: CGFloat(pixels) / logicalSize, y: CGFloat(pixels) / logicalSize)
    draw(context)
    let bitmap = NSBitmapImageRep(cgImage: context.makeImage()!)
    try bitmap.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent(filename))
}

func pdf(_ filename: String, size: CGFloat, draw: (CGContext) -> Void) {
    var box = CGRect(x: 0, y: 0, width: size, height: size)
    let consumer = CGDataConsumer(url: output.appendingPathComponent(filename) as CFURL)!
    let context = CGContext(consumer: consumer, mediaBox: &box, nil)!
    context.beginPDFPage(nil)
    draw(context)
    context.endPDFPage()
    context.closePDF()
}

try png("AppIcon.png", pixels: 1024, logicalSize: 1024, draw: drawApp)
pdf("AppIcon.pdf", size: 1024, draw: drawApp)
if !appOnly {
    pdf("MenuBarIconTemplate.pdf", size: 18) { drawMenu(in: $0) }
    try png("MenuBarIconTemplate.png", pixels: 18, logicalSize: 18) { drawMenu(in: $0) }
    try png("MenuBarIconTemplate@2x.png", pixels: 36, logicalSize: 18) { drawMenu(in: $0) }
}

let iconset = output.appendingPathComponent("AppIcon.iconset", isDirectory: true)
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
for points in [16, 32, 128, 256, 512] {
    try png("AppIcon.iconset/icon_\(points)x\(points).png", pixels: points, logicalSize: 1024, draw: drawApp)
    try png("AppIcon.iconset/icon_\(points)x\(points)@2x.png", pixels: points * 2, logicalSize: 1024, draw: drawApp)
}

let tile = appStyle == "negative" ? "#101820" : (appStyle == "cobalt" ? "#2563EB" : "#F0F3F7")
let body = appStyle == "negative" ? "#E8EFF5" : "#17212E"
let screen = appStyle == "negative" ? tile : (appStyle == "cobalt" ? "#F7FAFF" : "#DDE8F3")
let button = appStyle == "cobalt" ? "#55B6FF" : "#3478F6"
let bodySVG = appStyle == "outline"
    ? "<rect x=\"347\" y=\"187\" width=\"330\" height=\"650\" rx=\"53\" fill=\"none\" stroke=\"\(body)\" stroke-width=\"22\"/>"
    : "<rect x=\"336\" y=\"176\" width=\"352\" height=\"672\" rx=\"64\" fill=\"\(body)\"/>"
let appSVG = """
<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <title>ThinCube — front view, \(appStyle)</title>
  <rect x="80" y="80" width="864" height="864" rx="190" fill="\(tile)"/>
  \(bodySVG)
  <rect x="384" y="220" width="256" height="456" rx="24" fill="\(screen)"/>
  <rect x="424" y="744" width="176" height="28" rx="14" fill="\(button)"/>
</svg>
"""
let menuSVG = """
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 18 18">
  <title>ThinCube — menu bar device template</title>
  <path fill="currentColor" fill-rule="evenodd" d="M6.5 1H11.5Q13 1 13 2.5V15.5Q13 17 11.5 17H6.5Q5 17 5 15.5V2.5Q5 1 6.5 1Z M6 3H12V12H6Z M7 14H11V15H7Z"/>
</svg>
"""
try appSVG.write(to: output.appendingPathComponent("AppIcon.svg"), atomically: true, encoding: .utf8)
if !appOnly {
try menuSVG.write(to: output.appendingPathComponent("MenuBarIconTemplate.svg"), atomically: true, encoding: .utf8)

let preview = NSImage(size: NSSize(width: 360, height: 112))
preview.lockFocus()
let context = NSGraphicsContext.current!.cgContext
for (x, background, foreground, title) in [
    (CGFloat(0), NSColor(calibratedWhite: 0.95, alpha: 1), NSColor.black, "浅色菜单栏"),
    (CGFloat(180), NSColor(calibratedWhite: 0.10, alpha: 1), NSColor.white, "深色菜单栏")
] {
    background.setFill()
    NSRect(x: x, y: 0, width: 180, height: 112).fill()
    context.saveGState()
    context.translateBy(x: x + 63, y: 40)
    context.scaleBy(x: 3, y: 3)
    drawMenu(in: context, color: foreground.cgColor)
    context.restoreGState()
    (title as NSString).draw(at: NSPoint(x: x + 55, y: 14), withAttributes: [
        .font: NSFont.systemFont(ofSize: 12),
        .foregroundColor: foreground.withAlphaComponent(0.65)
    ])
}
preview.unlockFocus()
try NSBitmapImageRep(data: preview.tiffRepresentation!)!.representation(using: .png, properties: [:])!
    .write(to: output.appendingPathComponent("MenuBarIcon-preview.png"))
}

// A vector-rendered comparison board, including actual 32px / 64px proofs.
let board = NSImage(size: NSSize(width: 900, height: 416))
board.lockFocus()
let boardContext = NSGraphicsContext.current!.cgContext
NSColor(calibratedWhite: 0.98, alpha: 1).setFill()
NSRect(x: 0, y: 0, width: 900, height: 416).fill()
let labels = ["A · 深色负形", "B · 蓝底色块", "C · 浅色线框"]
for (index, style) in styles.enumerated() {
    let x = CGFloat(index * 300)
    for (size, y, dx) in [(CGFloat(256), CGFloat(120), CGFloat(22)), (64, 31, 112), (32, 47, 204)] {
        boardContext.saveGState()
        boardContext.translateBy(x: x + dx, y: y)
        boardContext.scaleBy(x: size / 1024, y: size / 1024)
        drawAppStyle(in: boardContext, style: style)
        boardContext.restoreGState()
    }
    (labels[index] as NSString).draw(at: NSPoint(x: x + 91, y: 101), withAttributes: [
        .font: NSFont.systemFont(ofSize: 14, weight: .medium), .foregroundColor: NSColor(calibratedWhite: 0.15, alpha: 1)
    ])
}
board.unlockFocus()
try NSBitmapImageRep(data: board.tiffRepresentation!)!.representation(using: .png, properties: [:])!
    .write(to: output.appendingPathComponent("Front-view-comparison.png"))
print("Flat ThinCube icons exported: editable SVG, PDF, PNG and full iconset")
