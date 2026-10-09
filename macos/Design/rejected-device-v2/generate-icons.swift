import AppKit
import CoreGraphics
import Foundation

// ThinCube: a thin StickS3-shaped cuboid, screen above a blue square button.
// The application and optical-size menu marks use flat vector geometry only.
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
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

func polygon(_ context: CGContext, _ points: [CGPoint], _ fill: CGColor) {
    context.setFillColor(fill)
    context.beginPath()
    context.move(to: points[0])
    for point in points.dropFirst() { context.addLine(to: point) }
    context.closePath()
    context.fillPath()
}

func drawApp(in context: CGContext) {
    context.saveGState()
    context.translateBy(x: 0, y: 1024)
    context.scaleBy(x: 1, y: -1)
    rounded(context, CGRect(x: 80, y: 80, width: 864, height: 864), 190, color(0xEEF2F7))
    polygon(context, [CGPoint(x: 360, y: 232), CGPoint(x: 408, y: 208),
                      CGPoint(x: 664, y: 208), CGPoint(x: 616, y: 232)], color(0x4B596B))
    polygon(context, [CGPoint(x: 616, y: 232), CGPoint(x: 664, y: 208),
                      CGPoint(x: 664, y: 784), CGPoint(x: 616, y: 808)], color(0x1B2433))
    // Shared straight edges keep the three cuboid faces joined without seams.
    rounded(context, CGRect(x: 360, y: 232, width: 256, height: 576), 0, color(0x354154))
    rounded(context, CGRect(x: 400, y: 272, width: 176, height: 312), 8, color(0x101824))
    rounded(context, CGRect(x: 448, y: 648, width: 80, height: 80), 10, color(0x278BFF))
    context.restoreGState()
}

func drawMenu(in context: CGContext, color: CGColor = CGColor(gray: 0, alpha: 1)) {
    context.saveGState()
    context.translateBy(x: 0, y: 18)
    context.scaleBy(x: 1, y: -1)
    context.setShouldAntialias(false)
    let path = CGMutablePath()
    path.addRoundedRect(in: CGRect(x: 5, y: 1, width: 8, height: 16), cornerWidth: 1, cornerHeight: 1)
    path.addRect(CGRect(x: 7, y: 3, width: 4, height: 8))
    path.addRect(CGRect(x: 8, y: 13, width: 2, height: 2))
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
pdf("MenuBarIconTemplate.pdf", size: 18) { drawMenu(in: $0) }
try png("MenuBarIconTemplate.png", pixels: 18, logicalSize: 18) { drawMenu(in: $0) }
try png("MenuBarIconTemplate@2x.png", pixels: 36, logicalSize: 18) { drawMenu(in: $0) }

let iconset = output.appendingPathComponent("AppIcon.iconset", isDirectory: true)
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
for points in [16, 32, 128, 256, 512] {
    try png("AppIcon.iconset/icon_\(points)x\(points).png", pixels: points, logicalSize: 1024, draw: drawApp)
    try png("AppIcon.iconset/icon_\(points)x\(points)@2x.png", pixels: points * 2, logicalSize: 1024, draw: drawApp)
}

let appSVG = """
<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <title>ThinCube — flat StickS3 device icon</title>
  <rect x="80" y="80" width="864" height="864" rx="190" fill="#EEF2F7"/>
  <path d="M360 232 408 208H664L616 232Z" fill="#4B596B"/>
  <path d="M616 232 664 208V784L616 808Z" fill="#1B2433"/>
  <rect x="360" y="232" width="256" height="576" fill="#354154"/>
  <rect x="400" y="272" width="176" height="312" rx="8" fill="#101824"/>
  <rect x="448" y="648" width="80" height="80" rx="10" fill="#278BFF"/>
</svg>
"""
let menuSVG = """
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 18 18">
  <title>ThinCube — menu bar device template</title>
  <path fill="currentColor" fill-rule="evenodd" d="M6 1H12Q13 1 13 2V16Q13 17 12 17H6Q5 17 5 16V2Q5 1 6 1Z M7 3H11V11H7Z M8 13H10V15H8Z"/>
</svg>
"""
try appSVG.write(to: output.appendingPathComponent("AppIcon.svg"), atomically: true, encoding: .utf8)
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
print("Flat ThinCube icons exported: editable SVG, PDF, PNG and full iconset")
