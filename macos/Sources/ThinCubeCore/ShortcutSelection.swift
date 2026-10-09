/// A manual shortcut draft. Opening or cancelling the chooser never changes
/// the live mapping; only a valid, explicitly applied draft becomes a Shortcut.
public struct ShortcutSelection {
    public var modifierCodes: Set<UInt16>
    public var keyCode: UInt16?
    public let distinguishSides: Bool

    public init(shortcut: Shortcut) {
        distinguishSides = shortcut.distinguishSides
        modifierCodes = Set(shortcut.combo?.modifierCodes ?? [])
        keyCode = shortcut.combo?.keyCode
    }

    public var shortcut: Shortcut? {
        guard modifierCodes.allSatisfy({ ShortcutCatalog.modifier($0) != nil }),
              keyCode.map({ ShortcutCatalog.modifier($0) == nil }) ?? true else { return nil }
        var parts = ShortcutCatalog.modifiers.filter { modifierCodes.contains($0.code) }.map {
            distinguishSides ? $0.token : $0.group
        }
        if let keyCode {
            parts.append(ShortcutCatalog.keys.first { $0.code == keyCode }?.token ?? "keycode_\(keyCode)")
        }
        guard let spec = ShortcutCatalog.canonicalSpec(parts.joined(separator: "+")) else { return nil }
        return Shortcut(spec: spec, distinguishSides: distinguishSides)
    }
}
