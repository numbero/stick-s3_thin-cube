import CoreGraphics

/// A keyboard edge ready to construct or inspect without posting an event.
public struct KeyEventStep: Equatable {
    public let code: UInt16
    public let down: Bool
    public let flags: CGEventFlags
    public var isModifier: Bool { ShortcutCatalog.modifier(code) != nil }
    public static let sourceTag: Int64 = 0x5443494e

    public func makeEvent(source: CGEventSource? = nil) -> CGEvent? {
        guard let event = CGEvent(keyboardEventSource: source, virtualKey: code, keyDown: down) else {
            return nil
        }
        event.type = isModifier ? .flagsChanged : (down ? .keyDown : .keyUp)
        // Preserve the key's native numeric-pad, function and help flags.
        // Replacing all flags would erase these and the left/right identity.
        event.flags.subtract(KeyEventSequence.modifierFlags.union(.maskAlphaShift))
        event.flags.formUnion(flags)
        event.setIntegerValueField(.keyboardEventAutorepeat, value: 0)
        event.setIntegerValueField(.eventSourceUserData, value: Self.sourceTag)
        return event
    }
}

/// Plans a held chord and its reverse release, independently of Accessibility
/// permission and CGEvent posting. Only keys pressed by this sequence are owned.
public struct KeyEventSequence {
    public private(set) var injectedCodes: [UInt16] = []
    private var owners: [DeviceButton: [UInt16]] = [:]
    public var isActive: Bool { !owners.isEmpty }
    public func isActive(for button: DeviceButton) -> Bool { owners[button] != nil }

    public init() {}

    static let modifierFlags: CGEventFlags = ShortcutCatalog.modifiers.reduce([]) {
        $0.union($1.flag).union(CGEventFlags(rawValue: $1.side))
    }

    /// nil rejects a duplicate press or an ordinary key already physically held.
    /// An empty plan is valid when all keys of a modifier-only chord are held.
    public mutating func press(_ combo: KeyCombo, physicallyHeld: Set<UInt16> = [],
                               physicalFlags: CGEventFlags = [], button: DeviceButton = .a) -> [KeyEventStep]? {
        let physical = physicallyHeld.subtracting(injectedCodes)
        guard owners[button] == nil, !combo.allCodes.isEmpty,
              combo.keyCode.map({ !physical.contains($0) }) ?? true else { return nil }
        owners[button] = combo.allCodes
        var steps: [KeyEventStep] = []
        for code in combo.allCodes where !physical.contains(code) && !injectedCodes.contains(code) {
            injectedCodes.append(code)
            steps.append(KeyEventStep(code: code, down: true,
                                      flags: flags(physicallyHeld: physical, physicalFlags: physicalFlags)))
        }
        return steps
    }

    public mutating func release(physicallyHeld: Set<UInt16> = [],
                                 physicalFlags: CGEventFlags = [], button: DeviceButton? = nil) -> [KeyEventStep] {
        var steps: [KeyEventStep] = []
        if let button { owners.removeValue(forKey: button) } else { owners.removeAll() }
        let retained = Set(owners.values.flatMap { $0 })
        // A CGEventSource state snapshot can contain our own posted keys.
        // Treating those as physical keeps their modifier flags stuck on the
        // synthetic up. Preserve other keys, but always clear our owned keys.
        let otherKeys = physicallyHeld.subtracting(injectedCodes)
        for code in injectedCodes.reversed() where !retained.contains(code) {
            injectedCodes.removeAll { $0 == code }
            steps.append(KeyEventStep(code: code, down: false,
                                      flags: flags(physicallyHeld: otherKeys, physicalFlags: physicalFlags)))
        }
        return steps
    }

    private func flags(physicallyHeld: Set<UInt16>, physicalFlags: CGEventFlags) -> CGEventFlags {
        var result = physicalFlags.subtracting(Self.modifierFlags)
        let held = physicallyHeld.union(injectedCodes)
        for modifier in ShortcutCatalog.modifiers where held.contains(modifier.code) {
            result.formUnion(modifier.flag)
            result.formUnion(CGEventFlags(rawValue: modifier.side))
        }
        return result
    }
}
