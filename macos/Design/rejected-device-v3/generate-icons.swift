import AppKit
import CoreGraphics
import Foundation

// ThinCube: official StickS3 proportions, rounded enclosure and blue bar button.
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
    rounded(context, CGRect(x: 80, y: 80, width: 864, height: 864), 190, color(0xDCEBFC))
    context.concatenate(CGAffineTransform(a: 0.90, b: -0.42, c: 0.55, d: 0.95, tx: 252, ty: 280))
    // Sweep the rounded case through its depth with a single uniform side color.
    // This preserves volume without lighting, gradients or a drop shadow.
    for step in stride(from: 96, through: 0, by: -1) {
        let t = CGFloat(step) / 96
        rounded(context, CGRect(x: -50 * t, y: 36 * t, width: 280, height: 560), 36, color(0x1E242D))
    }
    // A small flat slot on the bottom plane recalls the real connector.
    polygon(context, [CGPoint(x: 86, y: 568), CGPoint(x: 202, y: 568),
                      CGPoint(x: 187, y: 579), CGPoint(x: 71, y: 579)], color(0x0D1118))
    rounded(context, CGRect(x: 0, y: 0, width: 280, height: 560), 36, color(0x353C46))
    rounded(context, CGRect(x: 48, y: 36, width: 184, height: 328), 8, color(0xF8F7F1))
    rounded(context, CGRect(x: 74, y: 438, width: 132, height: 24), 12, color(0x1769FF))
    context.restoreGState()
}

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
pdf("MenuBarIconTemplate.pdf", size: 18) { drawMenu(in: $0) }
try png("MenuBarIconTemplate.png", pixels: 18, logicalSize: 18) { drawMenu(in: $0) }
try png("MenuBarIconTemplate@2x.png", pixels: 36, logicalSize: 18) { drawMenu(in: $0) }

let iconset = output.appendingPathComponent("AppIcon.iconset", isDirectory: true)
try FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
for points in [16, 32, 128, 256, 512] {
    try png("AppIcon.iconset/icon_\(points)x\(points).png", pixels: points, logicalSize: 1024, draw: drawApp)
    try png("AppIcon.iconset/icon_\(points)x\(points)@2x.png", pixels: points * 2, logicalSize: 1024, draw: drawApp)
}

let sideCopies = stride(from: 96, through: 0, by: -1).map { step -> String in
    let t = Double(step) / 96
    return "    <use href=\"#case\" transform=\"translate(\(-50 * t) \(36 * t))\"/>"
}.joined(separator: "\n")
let appSVG = """
<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <title>ThinCube — StickS3, flat illustration with volume</title>
  <defs><rect id="case" width="280" height="560" rx="36"/></defs>
  <rect x="80" y="80" width="864" height="864" rx="190" fill="#DCEBFC"/>
  <g transform="matrix(.90 -.42 .55 .95 252 280)">
    <g fill="#1E242D">
\(sideCopies)
    </g>
    <path d="M86 568H202L187 579H71Z" fill="#0D1118"/>
    <use href="#case" fill="#353C46"/>
    <rect x="48" y="36" width="184" height="328" rx="8" fill="#F8F7F1"/>
    <rect x="74" y="438" width="132" height="24" rx="12" fill="#1769FF"/>
  </g>
</svg>
"""
let menuSVG = """
<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 18 18">
  <title>ThinCube — menu bar device template</title>
  <path fill="currentColor" fill-rule="evenodd" d="M6.5 1H11.5Q13 1 13 2.5V15.5Q13 17 11.5 17H6.5Q5 17 5 15.5V2.5Q5 1 6.5 1Z M6 3H12V12H6Z M7 14H11V15H7Z"/>
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
