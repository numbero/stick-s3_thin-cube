import AppKit
import CoreGraphics
import XCTest
@testable import ThinCubeCore

final class ShortcutTests: XCTestCase {
    private func flags(_ codes: UInt16...) -> CGEventFlags { flags(codes) }

    private func flags(_ codes: [UInt16]) -> CGEventFlags {
        codes.reduce(into: CGEventFlags()) { result, code in
            if let modifier = ShortcutCatalog.modifier(code) {
                result.formUnion(modifier.flag)
                result.formUnion(CGEventFlags(rawValue: modifier.side))
            }
        }
    }

    func testEveryCatalogKeyAndModifierHasAStablePhysicalCode() throws {
        XCTAssertEqual(ShortcutCatalog.modifiers.count, 8)
        XCTAssertEqual(Set(ShortcutCatalog.keys.map(\.code)).count, ShortcutCatalog.keys.count)
        for key in ShortcutCatalog.keys {
            let combo = try XCTUnwrap(ShortcutCatalog.parse(key.token), key.token)
            XCTAssertEqual(combo.keyCode, key.code)
            XCTAssertEqual(combo.modifierCodes, [])
            XCTAssertEqual(ShortcutCatalog.spec(code: key.code, flags: [], distinguishSides: true), key.token)
        }
        for modifier in ShortcutCatalog.modifiers {
            let combo = try XCTUnwrap(ShortcutCatalog.parse(modifier.token), modifier.token)
            XCTAssertEqual(combo.modifierCodes, [modifier.code])
            XCTAssertNil(combo.keyCode)
            XCTAssertEqual(combo.allCodes, [modifier.code])
            XCTAssertNotEqual(modifier.side, 0)
        }
    }

    func testLeftAndRightModifiersRemainSeparateAndCanBeCollapsedWithoutLosingTheSavedSpec() throws {
        let shortcut = Shortcut(spec: "right_cmd + left_ctrl + left_cmd + f20", distinguishSides: true)
        XCTAssertEqual(shortcut.spec, "left_ctrl+left_cmd+right_cmd+f20")
        XCTAssertEqual(shortcut.combo?.modifierCodes, [59, 55, 54])
        XCTAssertEqual(shortcut.combo?.keyCode, 90)
        XCTAssertEqual(shortcut.combo?.allCodes, [59, 55, 54, 90])
        XCTAssertEqual(shortcut.label, "左 ⌃ + 左 ⌘ + 右 ⌘ + F20")
        var collapsed = shortcut
        collapsed.distinguishSides = false
        XCTAssertEqual(collapsed.effectiveSpec, "ctrl+cmd+f20")
        XCTAssertEqual(collapsed.combo?.modifierCodes, [59, 55])
        XCTAssertEqual(collapsed.label, "⌃ + ⌘ + F20")
        XCTAssertEqual(collapsed.spec, shortcut.spec)
        collapsed.distinguishSides = true
        XCTAssertEqual(collapsed, shortcut)
    }

    func testModifierOnlyChordsIncludeTheFinalModifierOnce() throws {
        let combo = try XCTUnwrap(ShortcutCatalog.parse("right_cmd+left_opt+left_cmd+right_cmd"))
        XCTAssertEqual(combo.modifierCodes, [58, 55, 54])
        XCTAssertNil(combo.keyCode)
        XCTAssertEqual(combo.allCodes, [58, 55, 54])
        XCTAssertEqual(ShortcutCatalog.ignoringSides("left_cmd+right_cmd"), "cmd")
        XCTAssertEqual(ShortcutCatalog.canonicalSpec("cmd+left_cmd"), "left_cmd")
        XCTAssertEqual(ShortcutCatalog.parse("cmd+right_cmd")?.modifierCodes, [55, 54])
    }

    func testFunctionKeysKeypadNavigationAndPunctuation() throws {
        for index in 1...20 { XCTAssertNotNil(ShortcutCatalog.parse("f\(index)")) }
        let samples: [(String, UInt16)] = [
            ("f20", 0x5A), ("keypad_decimal", 0x41), ("keypad_multiply", 0x43),
            ("keypad_plus", 0x45), ("keypad_clear", 0x47), ("keypad_divide", 0x4B),
            ("keypad_enter", 0x4C), ("keypad_minus", 0x4E), ("keypad_equal", 0x51),
            ("keypad_0", 0x52), ("keypad_9", 0x5C), ("home", 0x73),
            ("page_up", 0x74), ("forward_delete", 0x75), ("end", 0x77),
            ("page_down", 0x79), ("left", 0x7B), ("right", 0x7C), ("down", 0x7D), ("up", 0x7E),
            ("equal", 0x18), ("minus", 0x1B), ("left_bracket", 0x21), ("right_bracket", 0x1E),
            ("quote", 0x27), ("semicolon", 0x29), ("backslash", 0x2A),
            ("comma", 0x2B), ("slash", 0x2C), ("period", 0x2F), ("grave", 0x32), ("iso_section", 0x0A),
        ]
        for (token, code) in samples {
            let combo = try XCTUnwrap(ShortcutCatalog.parse("right_opt+\(token)"), token)
            XCTAssertEqual(combo.modifierCodes, [61], token)
            XCTAssertEqual(combo.keyCode, code, token)
            XCTAssertNotEqual(ShortcutCatalog.label(token), "未设置", token)
        }
    }

    func testFolotoyAliasesAreNormalized() {
        let aliases = [
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
        for (alias, canonical) in aliases {
            XCTAssertEqual(ShortcutCatalog.canonicalSpec(" \(alias.uppercased()) \n"), canonical, alias)
            XCTAssertEqual(ShortcutCatalog.parse(alias), ShortcutCatalog.parse(canonical), alias)
        }
        XCTAssertEqual(ShortcutCatalog.canonicalSpec(" ALT + COMMAND + Esc "), "opt+cmd+escape")
        XCTAssertTrue(ShortcutCatalog.hasSides("RCOMMAND"))
        XCTAssertFalse(ShortcutCatalog.hasSides("command"))
    }

    func testUnknownVirtualCodesAndUnsupportedSpecs() {
        XCTAssertEqual(ShortcutCatalog.canonicalSpec("keycode_00052"), "keycode_52")
        XCTAssertEqual(ShortcutCatalog.parse("keycode_52")?.keyCode, 52)
        XCTAssertEqual(ShortcutCatalog.label("keycode_52"), "键 52")
        XCTAssertEqual(ShortcutCatalog.canonicalSpec("keycode_49"), "space")
        XCTAssertEqual(ShortcutCatalog.canonicalSpec("keycode_54"), "right_cmd")
        for spec in ["", " ", "+", "cmd+", "+space", "cmd++space", "space+cmd", "a+b", "cmd+a+b",
                     "f21", "capslock", "fn", "keycode_57", "keycode_63", "keycode_128", "keycode_-1", "keycode_65536"] {
            XCTAssertNil(ShortcutCatalog.parse(spec), spec)
            XCTAssertEqual(ShortcutCatalog.label(spec), "未设置", spec)
            XCTAssertFalse(Shortcut(spec: spec).isValid, spec)
        }
    }

    func testFlagConversionPreservesBothSidesAndModifierOnlyChords() {
        let bothSides = flags(59, 62, 61, 56, 60, 55, 54)
        XCTAssertEqual(ShortcutCatalog.spec(code: 90, flags: bothSides, distinguishSides: true),
                       "left_ctrl+right_ctrl+right_opt+left_shift+right_shift+left_cmd+right_cmd+f20")
        XCTAssertEqual(ShortcutCatalog.spec(code: 90, flags: bothSides, distinguishSides: false),
                       "ctrl+opt+shift+cmd+f20")
        XCTAssertEqual(ShortcutCatalog.spec(code: 54, flags: flags(55, 54), distinguishSides: true),
                       "left_cmd+right_cmd")
        XCTAssertEqual(ShortcutCatalog.spec(code: 54, flags: flags(55, 54), distinguishSides: false), "cmd")
    }

    func testRecordingWaitsForTheOrdinaryKeyAndModifierInEitherReleaseOrder() {
        for releaseModifierFirst in [true, false] {
            var capture = ShortcutCapture()
            let held = flags(61)
            XCTAssertNil(capture.receive(type: .flagsChanged, code: 61, flags: held))
            XCTAssertNil(capture.receive(type: .keyDown, code: 49, flags: held))
            XCTAssertEqual(capture.candidate, "right_opt+space")
            if releaseModifierFirst {
                XCTAssertNil(capture.receive(type: .flagsChanged, code: 61, flags: []))
                XCTAssertEqual(capture.receive(type: .keyUp, code: 49, flags: []), "right_opt+space")
            } else {
                XCTAssertNil(capture.receive(type: .keyUp, code: 49, flags: held))
                XCTAssertEqual(capture.receive(type: .flagsChanged, code: 61, flags: []), "right_opt+space")
            }
            XCTAssertNil(capture.receive(type: .keyUp, code: 49, flags: []), "A completed recording fires only once")
        }
    }

    func testPureModifierRecordingPreservesBothSidesWhileEitherSideIsReleased() {
        for firstRelease: UInt16 in [55, 54] {
            var capture = ShortcutCapture()
            XCTAssertNil(capture.receive(type: .flagsChanged, code: 55, flags: flags(55)))
            XCTAssertNil(capture.receive(type: .flagsChanged, code: 54, flags: flags(55, 54)))
            XCTAssertEqual(capture.candidate, "left_cmd+right_cmd")
            let remaining: UInt16 = firstRelease == 55 ? 54 : 55
            XCTAssertNil(capture.receive(type: .flagsChanged, code: firstRelease, flags: flags(remaining)))
            XCTAssertEqual(capture.candidate, "left_cmd+right_cmd")
            XCTAssertEqual(capture.receive(type: .flagsChanged, code: remaining, flags: []), "left_cmd+right_cmd")
        }
        var capture = ShortcutCapture()
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 62, flags: flags(62)))
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 61, flags: flags(62, 61)))
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 61, flags: flags(62)))
        XCTAssertEqual(capture.receive(type: .flagsChanged, code: 62, flags: []), "right_ctrl+right_opt")
    }

    func testRecordingEscapeRepeatsAndAdditionalOrdinaryKeys() {
        var capture = ShortcutCapture()
        XCTAssertNil(capture.receive(type: .keyDown, code: 53, flags: [], isRepeat: true))
        XCTAssertNil(capture.candidate)
        XCTAssertNil(capture.receive(type: .keyDown, code: 53, flags: []))
        XCTAssertNil(capture.receive(type: .keyDown, code: 0, flags: []))
        XCTAssertNil(capture.receive(type: .keyUp, code: 53, flags: []))
        XCTAssertEqual(capture.receive(type: .keyUp, code: 0, flags: []), "escape")
    }

    func testCapsLockAndFnCannotCreateARecordingOrReplaceItsCandidate() {
        var capture = ShortcutCapture()
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 57, flags: .maskAlphaShift))
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 63, flags: .maskSecondaryFn))
        XCTAssertNil(capture.receive(type: .keyDown, code: 57, flags: []))
        XCTAssertNil(capture.receive(type: .keyDown, code: 63, flags: []))
        XCTAssertNil(capture.candidate)
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 54, flags: flags(54)))
        XCTAssertEqual(capture.candidate, "right_cmd")
        XCTAssertNil(capture.receive(type: .flagsChanged, code: 63, flags: .maskSecondaryFn))
        XCTAssertEqual(capture.candidate, "right_cmd")
        XCTAssertEqual(capture.receive(type: .flagsChanged, code: 54, flags: .maskAlphaShift), "right_cmd")
    }

    func testLegacyJSONPreservesTheOldInjectorsLeftModifiers() throws {
        let decoder = JSONDecoder()
        let legacy = Data(#"{"keyCode":49,"command":false,"option":true,"control":false,"shift":false}"#.utf8)
        let migrated = try decoder.decode(Shortcut.self, from: legacy)
        XCTAssertEqual(migrated, Shortcut(spec: "left_opt+space", distinguishSides: true))
        XCTAssertEqual(migrated.combo?.allCodes, [58, 49])
        let allModifiers = Data(#"{"keyCode":54,"command":true,"option":true,"control":true,"shift":true}"#.utf8)
        let bothCommands = try decoder.decode(Shortcut.self, from: allModifiers)
        XCTAssertEqual(bothCommands.spec, "left_ctrl+left_opt+left_shift+left_cmd+right_cmd")
        XCTAssertEqual(bothCommands.combo?.allCodes, [59, 58, 56, 55, 54])
        XCTAssertNil(bothCommands.combo?.keyCode)
        let duplicateMain = Data(#"{"keyCode":55,"command":true}"#.utf8)
        XCTAssertEqual(try decoder.decode(Shortcut.self, from: duplicateMain).combo?.allCodes, [55])
        XCTAssertThrowsError(try decoder.decode(Shortcut.self, from: Data(#"{"keyCode":57}"#.utf8)))
    }

    func testNewJSONRoundTripRetainsSidePreferenceAndWritesCanonicalSpecs() throws {
        let shortcut = Shortcut(spec: " RCOMMAND + LEFT_ALT + ESC ", distinguishSides: false)
        let data = try JSONEncoder().encode(shortcut)
        let decoded = try JSONDecoder().decode(Shortcut.self, from: data)
        XCTAssertEqual(decoded, shortcut)
        XCTAssertEqual(decoded.spec, "left_opt+right_cmd+escape")
        XCTAssertEqual(decoded.effectiveSpec, "opt+cmd+escape")
        let object = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [String: Any])
        XCTAssertEqual(Set(object.keys), ["spec", "distinguishSides"])
        var edited = Shortcut()
        edited.spec = " COMMAND + SPACE "
        let normalized = try JSONDecoder().decode(Shortcut.self, from: JSONEncoder().encode(edited))
        XCTAssertEqual(normalized.spec, "cmd+space")
        for bad in [#"{"spec":"cmd+a+b","distinguishSides":true}"#, #"{"spec":"keycode_63"}"#, #"{"spec":null}"#] {
            XCTAssertThrowsError(try JSONDecoder().decode(Shortcut.self, from: Data(bad.utf8)))
        }
        edited.spec = "bad"
        XCTAssertThrowsError(try JSONEncoder().encode(edited))
    }
}
