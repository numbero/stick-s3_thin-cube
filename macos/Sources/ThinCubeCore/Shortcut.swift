import AppKit
import CoreGraphics
import IOKit.hidsystem

/// A portable chord specification. Side information is retained when the user
/// temporarily turns side-sensitive playback off.
public struct Shortcut: Codable, Equatable {
    public var spec: String
    public var distinguishSides: Bool

    public init(spec: String = "opt+space", distinguishSides: Bool = false) {
        self.spec = ShortcutCatalog.canonicalSpec(spec) ?? spec
        self.distinguishSides = distinguishSides
    }

    public var effectiveSpec: String {
        distinguishSides ? spec : ShortcutCatalog.ignoringSides(spec)
    }
    public var label: String { ShortcutCatalog.label(effectiveSpec) }
    public var combo: KeyCombo? { ShortcutCatalog.parse(effectiveSpec) }
    public var isValid: Bool { combo != nil }

    private enum CodingKeys: String, CodingKey {
        case spec, distinguishSides, keyCode, command, option, control, shift
    }

    public init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        if values.contains(.spec) {
            let input = try values.decode(String.self, forKey: .spec)
            guard let canonical = ShortcutCatalog.canonicalSpec(input) else {
                throw DecodingError.dataCorruptedError(forKey: .spec, in: values,
                                                       debugDescription: "Invalid shortcut specification")
            }
            spec = canonical
            distinguishSides = try values.decodeIfPresent(Bool.self, forKey: .distinguishSides) ?? false
        } else {
            // The old injector explicitly pressed the left modifier keys.
            let code = try values.decode(UInt16.self, forKey: .keyCode)
            var parts: [String] = []
            for (field, token): (CodingKeys, String) in [
                (.control, "left_ctrl"), (.option, "left_opt"),
                (.shift, "left_shift"), (.command, "left_cmd")
            ] {
                if try values.decodeIfPresent(Bool.self, forKey: field) ?? false { parts.append(token) }
            }
            parts.append(ShortcutCatalog.modifier(code)?.token ?? "keycode_\(code)")
            guard let canonical = ShortcutCatalog.canonicalSpec(parts.joined(separator: "+")) else {
                throw DecodingError.dataCorruptedError(forKey: .keyCode, in: values,
                                                       debugDescription: "Invalid legacy shortcut key code")
            }
            spec = canonical
            distinguishSides = true
        }
    }

    public func encode(to encoder: Encoder) throws {
        guard let canonical = ShortcutCatalog.canonicalSpec(spec) else {
            throw EncodingError.invalidValue(spec, .init(codingPath: encoder.codingPath,
                                                        debugDescription: "Invalid shortcut specification"))
        }
        var values = encoder.container(keyedBy: CodingKeys.self)
        try values.encode(canonical, forKey: .spec)
        try values.encode(distinguishSides, forKey: .distinguishSides)
    }
}

/// The exact physical keys an injector should press. A modifier-only chord has
/// no ordinary keyCode; all its keys occur once in modifierCodes.
public struct KeyCombo: Equatable {
    public let modifierCodes: [UInt16]
    public let keyCode: UInt16?
    public var allCodes: [UInt16] { modifierCodes + (keyCode.map { [$0] } ?? []) }
}

public enum ShortcutCatalog {
    public struct Modifier {
        public let code: UInt16
        public let token: String
        public let label: String
        public let group: String
        public let flag: CGEventFlags
        public let side: UInt64
    }

    public struct Key {
        public let code: UInt16
        public let token: String
        public let label: String
    }

    public static let modifiers: [Modifier] = [
        .init(code: 0x3B, token: "left_ctrl", label: "左 ⌃", group: "ctrl", flag: .maskControl, side: UInt64(NX_DEVICELCTLKEYMASK)),
        .init(code: 0x3E, token: "right_ctrl", label: "右 ⌃", group: "ctrl", flag: .maskControl, side: UInt64(NX_DEVICERCTLKEYMASK)),
        .init(code: 0x3A, token: "left_opt", label: "左 ⌥", group: "opt", flag: .maskAlternate, side: UInt64(NX_DEVICELALTKEYMASK)),
        .init(code: 0x3D, token: "right_opt", label: "右 ⌥", group: "opt", flag: .maskAlternate, side: UInt64(NX_DEVICERALTKEYMASK)),
        .init(code: 0x38, token: "left_shift", label: "左 ⇧", group: "shift", flag: .maskShift, side: UInt64(NX_DEVICELSHIFTKEYMASK)),
        .init(code: 0x3C, token: "right_shift", label: "右 ⇧", group: "shift", flag: .maskShift, side: UInt64(NX_DEVICERSHIFTKEYMASK)),
        .init(code: 0x37, token: "left_cmd", label: "左 ⌘", group: "cmd", flag: .maskCommand, side: UInt64(NX_DEVICELCMDKEYMASK)),
        .init(code: 0x36, token: "right_cmd", label: "右 ⌘", group: "cmd", flag: .maskCommand, side: UInt64(NX_DEVICERCMDKEYMASK)),
    ]
    public static let modifierMask: CGEventFlags = [.maskCommand, .maskShift, .maskControl, .maskAlternate]
    private static let groups = ["ctrl", "opt", "shift", "cmd"]
    private static let symbols = ["ctrl": "⌃", "opt": "⌥", "shift": "⇧", "cmd": "⌘"]
    private static let keyDefinitions: [UInt16: (String, String)] = [
        0x00: ("a", "A"), 0x01: ("s", "S"), 0x02: ("d", "D"), 0x03: ("f", "F"),
        0x04: ("h", "H"), 0x05: ("g", "G"), 0x06: ("z", "Z"), 0x07: ("x", "X"),
        0x08: ("c", "C"), 0x09: ("v", "V"), 0x0A: ("iso_section", "§"), 0x0B: ("b", "B"),
        0x0C: ("q", "Q"), 0x0D: ("w", "W"), 0x0E: ("e", "E"), 0x0F: ("r", "R"),
        0x10: ("y", "Y"), 0x11: ("t", "T"), 0x12: ("1", "1"), 0x13: ("2", "2"),
        0x14: ("3", "3"), 0x15: ("4", "4"), 0x16: ("6", "6"), 0x17: ("5", "5"),
        0x18: ("equal", "="), 0x19: ("9", "9"), 0x1A: ("7", "7"), 0x1B: ("minus", "−"),
        0x1C: ("8", "8"), 0x1D: ("0", "0"), 0x1E: ("right_bracket", "]"), 0x1F: ("o", "O"),
        0x20: ("u", "U"), 0x21: ("left_bracket", "["), 0x22: ("i", "I"), 0x23: ("p", "P"),
        0x24: ("return", "Return ↩"), 0x25: ("l", "L"), 0x26: ("j", "J"), 0x27: ("quote", "'"),
        0x28: ("k", "K"), 0x29: ("semicolon", ";"), 0x2A: ("backslash", "\\"), 0x2B: ("comma", ","),
        0x2C: ("slash", "/"), 0x2D: ("n", "N"), 0x2E: ("m", "M"), 0x2F: ("period", "."),
        0x30: ("tab", "Tab ⇥"), 0x31: ("space", "空格"), 0x32: ("grave", "`"),
        0x33: ("delete", "Delete ⌫"), 0x35: ("escape", "Esc"),
        0x40: ("f17", "F17"), 0x41: ("keypad_decimal", "小键盘 ."), 0x43: ("keypad_multiply", "小键盘 ×"),
        0x45: ("keypad_plus", "小键盘 +"), 0x47: ("keypad_clear", "Clear"), 0x4B: ("keypad_divide", "小键盘 ÷"),
        0x4C: ("keypad_enter", "Enter ⌤"), 0x4E: ("keypad_minus", "小键盘 −"), 0x4F: ("f18", "F18"),
        0x50: ("f19", "F19"), 0x51: ("keypad_equal", "小键盘 ="),
        0x52: ("keypad_0", "小键盘 0"), 0x53: ("keypad_1", "小键盘 1"), 0x54: ("keypad_2", "小键盘 2"),
        0x55: ("keypad_3", "小键盘 3"), 0x56: ("keypad_4", "小键盘 4"), 0x57: ("keypad_5", "小键盘 5"),
        0x58: ("keypad_6", "小键盘 6"), 0x59: ("keypad_7", "小键盘 7"), 0x5A: ("f20", "F20"),
        0x5B: ("keypad_8", "小键盘 8"), 0x5C: ("keypad_9", "小键盘 9"),
        0x60: ("f5", "F5"), 0x61: ("f6", "F6"), 0x62: ("f7", "F7"), 0x63: ("f3", "F3"),
        0x64: ("f8", "F8"), 0x65: ("f9", "F9"), 0x67: ("f11", "F11"), 0x69: ("f13", "F13"),
        0x6A: ("f16", "F16"), 0x6B: ("f14", "F14"), 0x6D: ("f10", "F10"), 0x6F: ("f12", "F12"),
        0x71: ("f15", "F15"), 0x72: ("help", "Help"), 0x73: ("home", "Home ↖"),
        0x74: ("page_up", "Page Up ⇞"), 0x75: ("forward_delete", "Delete ⌦"), 0x76: ("f4", "F4"),
        0x77: ("end", "End ↘"), 0x78: ("f2", "F2"), 0x79: ("page_down", "Page Down ⇟"),
        0x7A: ("f1", "F1"), 0x7B: ("left", "←"), 0x7C: ("right", "→"), 0x7D: ("down", "↓"), 0x7E: ("up", "↑"),
    ]
    public static let keys: [Key] = keyDefinitions.sorted { $0.key < $1.key }.map {
        Key(code: $0.key, token: $0.value.0, label: $0.value.1)
    }
    private static let aliases: [String: String] = [
        "command": "cmd", "control": "ctrl", "option": "opt", "alt": "opt",
        "enter": "return", "esc": "escape", "backspace": "delete",
        "lcmd": "left_cmd", "lcommand": "left_cmd", "left_command": "left_cmd",
        "rcmd": "right_cmd", "rcommand": "right_cmd", "right_command": "right_cmd",
        "lshift": "left_shift", "rshift": "right_shift",
        "lopt": "left_opt", "lalt": "left_opt", "left_alt": "left_opt",
        "ropt": "right_opt", "ralt": "right_opt", "right_alt": "right_opt",
        "lctrl": "left_ctrl", "lcontrol": "left_ctrl", "left_control": "left_ctrl",
        "rctrl": "right_ctrl", "rcontrol": "right_ctrl", "right_control": "right_ctrl",
    ]

    public static func modifier(_ code: UInt16) -> Modifier? { modifiers.first { $0.code == code } }

    private static func keyCode(_ token: String) -> UInt16? {
        if let mod = modifiers.first(where: { $0.token == token || $0.group == token }) { return mod.code }
        if let key = keys.first(where: { $0.token == token }) { return key.code }
        if token.hasPrefix("keycode_"), let code = UInt16(token.dropFirst(8)), code < 128,
           ![0x39, 0x3F].contains(code) { return code }
        return nil
    }

    private static func canonicalParts(_ spec: String) -> [String]? {
        let tokens = spec.lowercased().split(separator: "+", omittingEmptySubsequences: false).map {
            let token = $0.trimmingCharacters(in: .whitespacesAndNewlines)
            return aliases[token] ?? token
        }
        guard !tokens.isEmpty else { return nil }
        var modifierTokens: [UInt16: String] = [:]
        var ordinary: String?
        for (index, token) in tokens.enumerated() {
            guard let code = keyCode(token) else { return nil }
            if let mod = modifier(code) {
                // A generic modifier maps to its left key. Prefer an explicit
                // side if the same physical key was also named explicitly.
                let canonical = token == mod.group ? mod.group : mod.token
                if modifierTokens[code] == nil || canonical == mod.token { modifierTokens[code] = canonical }
            } else {
                guard index == tokens.count - 1 else { return nil }
                ordinary = keyDefinitions[code]?.0 ?? "keycode_\(code)"
            }
        }
        var result = modifiers.compactMap { modifierTokens[$0.code] }
        if let ordinary { result.append(ordinary) }
        return result.isEmpty ? nil : result
    }

    public static func canonicalSpec(_ spec: String) -> String? { canonicalParts(spec)?.joined(separator: "+") }

    public static func parse(_ spec: String) -> KeyCombo? {
        guard let parts = canonicalParts(spec) else { return nil }
        let codes = parts.compactMap(keyCode)
        return KeyCombo(modifierCodes: codes.filter { modifier($0) != nil },
                        keyCode: codes.first { modifier($0) == nil })
    }

    public static func spec(code: UInt16, flags: CGEventFlags, distinguishSides: Bool) -> String {
        let main = modifier(code)
        var parts: [String] = []
        for group in groups {
            let pair = modifiers.filter { $0.group == group }
            let present = pair.filter { flags.rawValue & $0.side != 0 }
            if main?.group == group {
                if distinguishSides { parts += present.filter { $0.code != code }.map(\.token) }
            } else if distinguishSides && !present.isEmpty {
                parts += present.map(\.token)
            } else if flags.contains(pair[0].flag) {
                parts.append(group)
            }
        }
        parts.append(main.map { distinguishSides ? $0.token : $0.group } ?? keyDefinitions[code]?.0 ?? "keycode_\(code)")
        let result = parts.joined(separator: "+")
        return canonicalSpec(result) ?? result
    }

    public static func ignoringSides(_ spec: String) -> String {
        guard let parts = canonicalParts(spec) else { return spec }
        let generic = parts.map { token in
            modifiers.first(where: { $0.token == token })?.group ?? token
        }.joined(separator: "+")
        return canonicalSpec(generic) ?? spec
    }

    public static func hasSides(_ spec: String) -> Bool {
        guard let parts = canonicalParts(spec) else { return false }
        return parts.contains { token in modifiers.contains { $0.token == token } }
    }

    public static func label(_ spec: String) -> String {
        guard let parts = canonicalParts(spec) else { return "未设置" }
        return parts.map { token in
            if let symbol = symbols[token] { return symbol }
            if let mod = modifiers.first(where: { $0.token == token }) { return mod.label }
            if let code = keyCode(token) { return keyDefinitions[code]?.1 ?? "键 \(code)" }
            return token
        }.joined(separator: " + ")
    }
}

/// Records one chord, preserving its maximum modifier combination until every
/// participating key has been released.
public struct ShortcutCapture {
    public private(set) var candidate: String?
    private var mainKey: UInt16?
    private var heldKeys: Set<UInt16> = []
    private var peakModifierCount = 0
    private var completed = false

    public init() {}

    public mutating func receive(type: NSEvent.EventType, code: UInt16, flags: CGEventFlags,
                                 isRepeat: Bool = false) -> String? {
        guard !completed else { return nil }
        switch type {
        case .keyDown:
            guard !isRepeat, code < 128, ![0x39, 0x3F].contains(code) else { return nil }
            heldKeys.insert(code)
            if mainKey == nil {
                mainKey = code
                candidate = ShortcutCatalog.spec(code: code, flags: flags, distinguishSides: true)
            }
        case .keyUp:
            heldKeys.remove(code)
        case .flagsChanged:
            guard let mod = ShortcutCatalog.modifier(code) else { return nil }
            if mainKey == nil, flags.contains(mod.flag) {
                let pair = ShortcutCatalog.modifiers.filter { $0.group == mod.group }
                let sideBits = pair.reduce(UInt64(0)) { $0 | $1.side }
                let down = flags.rawValue & mod.side != 0 || flags.rawValue & sideBits == 0
                let count = ShortcutCatalog.modifiers.filter { flags.rawValue & $0.side != 0 }.count
                if down && count >= peakModifierCount {
                    peakModifierCount = count
                    candidate = ShortcutCatalog.spec(code: code, flags: flags, distinguishSides: true)
                }
            }
        default:
            return nil
        }
        let sideMask = ShortcutCatalog.modifiers.reduce(UInt64(0)) { $0 | $1.side }
        guard heldKeys.isEmpty, flags.intersection(ShortcutCatalog.modifierMask).isEmpty,
              flags.rawValue & sideMask == 0, let candidate,
              ShortcutCatalog.parse(candidate) != nil else { return nil }
        completed = true
        return candidate
    }
}
