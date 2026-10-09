import AppKit
import Foundation

struct Theme: Decodable {
    let id: String
    let name: String
    let background: String
    let contour: String
    let screen: String
    let button: String
    let screenMode: String
    let outputDir: String
}
let directory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let artifacts = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)
let themes = try JSONDecoder().decode([Theme].self, from: Data(contentsOf: directory.appendingPathComponent("manifest.json")))

func color(_ hex: String) -> NSColor {
    let rgb = UInt32(hex.dropFirst(), radix: 16)!
    return NSColor(srgbRed: CGFloat((rgb >> 16) & 255) / 255,
                   green: CGFloat((rgb >> 8) & 255) / 255,
                   blue: CGFloat(rgb & 255) / 255, alpha: 1)
}
func center(_ text: String, font: NSFont, tint: NSColor, x: CGFloat, y: CGFloat) {
    let label = text as NSString
    let attributes: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: tint]
    label.draw(at: NSPoint(x: x + (300 - label.size(withAttributes: attributes).width) / 2, y: y), withAttributes: attributes)
}
func drawRow(_ themes: ArraySlice<Theme>, y: CGFloat) {
    for (index, theme) in themes.enumerated() {
        let x = CGFloat(index * 300)
        let icon = NSImage(contentsOf: directory.appendingPathComponent(theme.outputDir).appendingPathComponent("AppIcon.png"))!
        icon.draw(in: NSRect(x: x + 10, y: y + 148, width: 280, height: 280))
        center("\(theme.id) · \(theme.name)", font: .systemFont(ofSize: 23, weight: .medium), tint: color("#28313A"), x: x, y: y + 118)
        let subtitle = theme.screenMode == "outline" ? "空心屏幕" : "实色屏幕"
        center(subtitle, font: .systemFont(ofSize: 13), tint: color("#87909A"), x: x, y: y + 96)
        for (offset, swatch) in [theme.background, theme.contour, theme.screen, theme.button].enumerated() {
            color(swatch).setFill()
            NSBezierPath(ovalIn: NSRect(x: x + 109 + CGFloat(offset * 22), y: y + 70, width: 16, height: 16)).fill()
        }
        icon.draw(in: NSRect(x: x + 94, y: y, width: 64, height: 64))
        icon.draw(in: NSRect(x: x + 174, y: y + 16, width: 32, height: 32))
    }
}
func export(_ name: String, rows: [ArraySlice<Theme>]) throws {
    let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 1200, pixelsHigh: rows.count * 450,
                                 bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                                 colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
    NSColor(srgbRed: 0.98, green: 0.982, blue: 0.986, alpha: 1).setFill()
    NSRect(x: 0, y: 0, width: 1200, height: rows.count * 450).fill()
    for (index, row) in rows.enumerated() {
        drawRow(row, y: CGFloat((rows.count - index - 1) * 450))
    }
    NSGraphicsContext.restoreGraphicsState()
    try bitmap.representation(using: .png, properties: [:])!.write(to: artifacts.appendingPathComponent(name))
}
try export("ThinCube-v6-A-D.png", rows: [themes[0..<4]])
try export("ThinCube-v6-E-H.png", rows: [themes[4..<8]])
try export("ThinCube-v6-themes.png", rows: [themes[0..<4], themes[4..<8]])
print("Exported eight-theme comparison boards")
