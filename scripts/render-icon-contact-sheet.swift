import AppKit
import Foundation

struct Concept: Decodable { let id: String; let name: String; let outputDir: String }
let directory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
let artifacts = URL(fileURLWithPath: CommandLine.arguments[2], isDirectory: true)
let concepts = try JSONDecoder().decode([Concept].self, from: Data(contentsOf: directory.appendingPathComponent("manifest.json")))

func drawRow(_ context: CGContext, concepts: ArraySlice<Concept>, offset: CGFloat) {
    for (index, concept) in concepts.enumerated() {
        let x = CGFloat(index * 240)
        let folder = directory.appendingPathComponent(concept.outputDir)
        let icon = NSImage(contentsOf: folder.appendingPathComponent("AppIcon.png"))!
        icon.draw(in: NSRect(x: x + 8, y: offset + 142, width: 224, height: 224))
        let title = "\(concept.id) · \(concept.name)" as NSString
        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: 22, weight: .medium),
            .foregroundColor: NSColor(calibratedWhite: 0.15, alpha: 1)
        ]
        let size = title.size(withAttributes: attributes)
        title.draw(at: NSPoint(x: x + (240 - size.width) / 2, y: offset + 108), withAttributes: attributes)
        icon.draw(in: NSRect(x: x + 68, y: offset + 24, width: 64, height: 64))
        icon.draw(in: NSRect(x: x + 156, y: offset + 40, width: 32, height: 32))
    }
}

func export(_ filename: String, rows: [ArraySlice<Concept>]) throws {
    let image = NSImage(size: NSSize(width: 1200, height: rows.count * 400))
    image.lockFocus()
    NSColor(calibratedWhite: 0.98, alpha: 1).setFill()
    NSRect(origin: .zero, size: image.size).fill()
    for (index, row) in rows.enumerated() {
        drawRow(NSGraphicsContext.current!.cgContext, concepts: row, offset: CGFloat((rows.count - index - 1) * 400))
    }
    image.unlockFocus()
    try NSBitmapImageRep(data: image.tiffRepresentation!)!.representation(using: .png, properties: [:])!
        .write(to: artifacts.appendingPathComponent(filename))
}

try export("ThinCube-v5-01-05.png", rows: [concepts[0..<5]])
try export("ThinCube-v5-06-10.png", rows: [concepts[5..<10]])
try export("ThinCube-v5-all.png", rows: [concepts[0..<5], concepts[5..<10]])
print("Contact sheets exported")
