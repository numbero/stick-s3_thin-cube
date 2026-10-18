import CoreGraphics
import XCTest
@testable import ThinCubeCore

final class KeyEventSequenceTests: XCTestCase {
    func testPulseContainsBothEdgesAndLeavesNoHeldOwner() throws {
        var sequence = KeyEventSequence()
        for _ in 0..<3 {
            let steps = try XCTUnwrap(sequence.pulse(combo("right_cmd+z")))
            XCTAssertEqual(steps.map(\.code), [54, 6, 6, 54])
            XCTAssertEqual(steps.map(\.down), [true, true, false, false])
            XCTAssertEqual(steps.last?.flags, [])
            XCTAssertFalse(sequence.isActive); XCTAssertTrue(sequence.injectedCodes.isEmpty)
        }
    }
    func testPulsePreservesABAndPhysicalKeysAndRejectsAnOwnedMainKey() throws {
        var sequence = KeyEventSequence()
        _ = try XCTUnwrap(sequence.press(combo("right_cmd+a"), button: .a))
        let steps = try XCTUnwrap(sequence.pulse(combo("right_cmd+z"), physicallyHeld: [54, 0, 56]))
        XCTAssertEqual(steps.map(\.code), [6, 6]); XCTAssertEqual(steps.last?.flags, flags([54, 56]))
        XCTAssertTrue(sequence.isActive(for: .a)); XCTAssertFalse(sequence.isActive(for: .doubleFlick))
        XCTAssertNil(sequence.pulse(try combo("right_cmd+a")))
        XCTAssertNil(sequence.pulse(try combo("cmd+b"), physicallyHeld: [11]))
        XCTAssertEqual(sequence.release(button: .a).map(\.code), [0, 54])
        XCTAssertTrue(try XCTUnwrap(sequence.pulse(combo("right_cmd"), physicallyHeld: [54])).isEmpty)
        XCTAssertFalse(sequence.isActive)
    }
    func testTwoButtonsShareModifiersAndReleaseIndependently() throws {
        var sequence = KeyEventSequence()
        _ = try XCTUnwrap(sequence.press(combo("right_cmd+a"), button: .a))
        let b = try XCTUnwrap(sequence.press(combo("right_cmd+b"), physicallyHeld: [54, 0], button: .b))
        XCTAssertEqual(b.map(\.code), [11])
        let aUp = sequence.release(physicallyHeld: [54, 0, 11], button: .a)
        XCTAssertEqual(aUp.map(\.code), [0]); XCTAssertEqual(aUp[0].flags, flags([54]))
        XCTAssertTrue(sequence.isActive(for: .b)); XCTAssertFalse(sequence.isActive(for: .a))
        let bUp = sequence.release(physicallyHeld: [54, 11], button: .b)
        XCTAssertEqual(bUp.map(\.code), [11, 54]); XCTAssertEqual(bUp.last?.flags, [])
        XCTAssertFalse(sequence.isActive)
    }

    func testSameChordBelongsToBothUntilLastButtonReleases() throws {
        var sequence = KeyEventSequence()
        let chord = try combo("right_cmd+return")
        _ = try XCTUnwrap(sequence.press(chord, button: .a))
        XCTAssertTrue(try XCTUnwrap(sequence.press(chord, physicallyHeld: [54, 36], button: .b)).isEmpty)
        XCTAssertTrue(sequence.release(physicallyHeld: [54, 36], button: .a).isEmpty)
        XCTAssertEqual(sequence.release(physicallyHeld: [54, 36], button: .b).map(\.code), [36, 54])
    }

    func testReleaseAllCleansBothButtonsAndOppositeModifierSides() throws {
        var sequence = KeyEventSequence()
        _ = try XCTUnwrap(sequence.press(combo("left_cmd+a"), button: .a))
        _ = try XCTUnwrap(sequence.press(combo("right_cmd+b"), button: .b))
        let up = sequence.release(physicallyHeld: [55, 0, 54, 11, 56])
        XCTAssertEqual(Set(up.map(\.code)), [55, 0, 54, 11])
        XCTAssertEqual(up.last?.flags, flags([56]))
        XCTAssertFalse(sequence.isActive)
        XCTAssertTrue(sequence.release(button: .a).isEmpty)
        XCTAssertTrue(sequence.release(button: .b).isEmpty)
    }

    func testRejectedPhysicalBDoesNotDisturbHeldA() throws {
        var sequence = KeyEventSequence()
        _ = try XCTUnwrap(sequence.press(combo("right_cmd+a"), button: .a))
        XCTAssertNil(sequence.press(try combo("cmd+b"), physicallyHeld: [54, 0, 11], button: .b))
        XCTAssertTrue(sequence.isActive(for: .a)); XCTAssertFalse(sequence.isActive(for: .b))
        XCTAssertTrue(sequence.release(button: .b).isEmpty)
        XCTAssertEqual(sequence.release(button: .a).map(\.code), [0, 54])
    }

    private func combo(_ spec: String) throws -> KeyCombo {
        try XCTUnwrap(ShortcutCatalog.parse(spec))
    }

    private func flags(_ codes: [UInt16]) -> CGEventFlags {
        codes.reduce(CGEventFlags()) { result, code in
            guard let modifier = ShortcutCatalog.modifier(code) else { return result }
            return result.union(modifier.flag).union(CGEventFlags(rawValue: modifier.side))
        }
    }

    func testEverySideGetsBothEdgesAndItsOwnFlags() throws {
        for modifier in ShortcutCatalog.modifiers {
            var sequence = KeyEventSequence()
            let down = try XCTUnwrap(sequence.press(combo(modifier.token)))
            XCTAssertEqual(down.count, 1, modifier.token)
            XCTAssertEqual(down.first?.code, modifier.code, modifier.token)
            XCTAssertEqual(down.first?.down, true, modifier.token)
            XCTAssertEqual(down.first?.flags, flags([modifier.code]), modifier.token)
            XCTAssertEqual(down.first?.isModifier, true, modifier.token)
            let up = sequence.release()
            XCTAssertEqual(up.count, 1, modifier.token)
            XCTAssertEqual(up.first?.code, modifier.code, modifier.token)
            XCTAssertEqual(up.first?.down, false, modifier.token)
            XCTAssertEqual(up.first?.flags, [], modifier.token)
        }
    }

    func testOrdinaryKeyFollowsModifiersAndReleasesBeforeThem() throws {
        var sequence = KeyEventSequence()
        let down = try XCTUnwrap(sequence.press(combo("right_ctrl+right_opt+right_shift+right_cmd+z")))
        XCTAssertEqual(down.map(\.code), [62, 61, 60, 54, 6])
        for (index, step) in down.enumerated() {
            XCTAssertTrue(step.down)
            XCTAssertEqual(step.flags, flags(Array([UInt16(62), 61, 60, 54].prefix(index + 1))))
        }
        let up = sequence.release()
        XCTAssertEqual(up.map(\.code), down.map(\.code).reversed())
        XCTAssertTrue(up.allSatisfy { !$0.down })
        XCTAssertEqual(up[0].flags, flags([62, 61, 60, 54]))
        XCTAssertEqual(up[1].flags, flags([62, 61, 60]))
        XCTAssertEqual(up.last?.flags, [])
        XCTAssertFalse(sequence.isActive)
    }

    func testReleasingOneSideKeepsTheOtherSideAndGenericFlag() throws {
        for group in ["ctrl", "opt", "shift", "cmd"] {
            let pair = ShortcutCatalog.modifiers.filter { $0.group == group }
            var sequence = KeyEventSequence()
            let down = try XCTUnwrap(sequence.press(combo(pair.map(\.token).joined(separator: "+"))))
            XCTAssertEqual(down.map(\.code), pair.map(\.code))
            XCTAssertEqual(down.last?.flags, flags(pair.map(\.code)))
            let up = sequence.release()
            XCTAssertEqual(up.first?.code, pair[1].code)
            XCTAssertEqual(up.first?.flags, flags([pair[0].code]))
            XCTAssertEqual(up.last?.flags, [])
        }
    }

    func testPhysicalModifiersAreIncludedButNeverOwned() throws {
        var sequence = KeyEventSequence()
        let down = try XCTUnwrap(sequence.press(combo("right_cmd+z"), physicallyHeld: [54, 56],
                                               physicalFlags: .maskAlphaShift))
        XCTAssertEqual(down.map(\.code), [6])
        XCTAssertEqual(down[0].flags, flags([54, 56]).union(.maskAlphaShift))
        let up = sequence.release(physicallyHeld: [54, 56], physicalFlags: .maskAlphaShift)
        XCTAssertEqual(up.map(\.code), [6])
        XCTAssertEqual(up[0].flags, flags([54, 56]).union(.maskAlphaShift))
    }

    func testPhysicalOppositeSideSurvivesSyntheticRelease() throws {
        var sequence = KeyEventSequence()
        let down = try XCTUnwrap(sequence.press(combo("right_cmd"), physicallyHeld: [55]))
        XCTAssertEqual(down[0].flags, flags([54, 55]))
        let up = sequence.release(physicallyHeld: [55])
        XCTAssertEqual(up[0].flags, flags([55]))
    }

    func testBorrowedModifierReleasedBeforeCleanupIsNotSynthesized() throws {
        var sequence = KeyEventSequence()
        let down = try XCTUnwrap(sequence.press(combo("right_cmd+z"), physicallyHeld: [54]))
        XCTAssertEqual(down[0].flags, flags([54]))
        let up = sequence.release()
        XCTAssertEqual(up.map(\.code), [6])
        XCTAssertEqual(up[0].flags, [])
    }

    func testOwnPostedKeysInSystemSnapshotCannotKeepModifiersStuck() throws {
        var sequence = KeyEventSequence()
        _ = try XCTUnwrap(sequence.press(combo("right_cmd+z")))
        let up = sequence.release(physicallyHeld: [54, 6, 56])
        XCTAssertEqual(up.map(\.code), [6, 54])
        XCTAssertTrue(up.allSatisfy { !$0.down })
        XCTAssertEqual(up[0].flags, flags([54, 56]))
        XCTAssertEqual(up[1].flags, flags([56]))
        XCTAssertFalse(sequence.isActive)
        XCTAssertTrue(sequence.injectedCodes.isEmpty)
        XCTAssertTrue(sequence.release().isEmpty)
    }

    func testRepeatedModifierPressesAlwaysFinishWithClearFlagsEvenIfSnapshotIncludesTheSyntheticDown() throws {
        for modifier in ShortcutCatalog.modifiers {
            var sequence = KeyEventSequence()
            for _ in 0..<3 {
                let down = try XCTUnwrap(sequence.press(combo(modifier.token)))
                XCTAssertEqual(down.last?.flags, flags([modifier.code]))
                let up = sequence.release(physicallyHeld: [modifier.code])
                XCTAssertEqual(up.last?.flags, [])
                XCTAssertFalse(sequence.isActive)
            }
        }
    }

    func testAlreadyHeldOrdinaryKeyRejectsWholeChord() throws {
        var sequence = KeyEventSequence()
        XCTAssertNil(sequence.press(try combo("cmd+z"), physicallyHeld: [6]))
        XCTAssertFalse(sequence.isActive)
        XCTAssertTrue(sequence.injectedCodes.isEmpty)
        XCTAssertTrue(sequence.release().isEmpty)
    }

    func testDuplicatePressIsRejectedAndReleaseIsIdempotent() throws {
        var sequence = KeyEventSequence()
        _ = try XCTUnwrap(sequence.press(combo("right_cmd")))
        XCTAssertNil(sequence.press(try combo("return")))
        XCTAssertEqual(sequence.release().map(\.code), [54])
        XCTAssertTrue(sequence.release().isEmpty)
        XCTAssertNotNil(sequence.press(try combo("return")))
    }

    func testAlreadyHeldModifierOnlyChordHasNoSyntheticEdges() throws {
        var sequence = KeyEventSequence()
        let down = try XCTUnwrap(sequence.press(combo("right_cmd"), physicallyHeld: [54]))
        XCTAssertTrue(down.isEmpty)
        XCTAssertTrue(sequence.isActive)
        XCTAssertNil(sequence.press(try combo("right_cmd")))
        XCTAssertTrue(sequence.release(physicallyHeld: [54]).isEmpty)
    }

    func testConstructedEventsKeepSidesAndNativeSpecialKeyFlagsWithoutPosting() throws {
        for spec in ["right_cmd", "right_ctrl+right_opt+right_shift+right_cmd+keypad_enter", "left", "f1"] {
            var sequence = KeyEventSequence()
            let steps = try XCTUnwrap(sequence.press(combo(spec))) + sequence.release()
            for step in steps {
                // A private state source can be unavailable in a sandbox. nil
                // still allows event inspection and never posts to the system.
                let event = try XCTUnwrap(step.makeEvent())
                XCTAssertEqual(event.type, step.isModifier ? .flagsChanged : (step.down ? .keyDown : .keyUp))
                XCTAssertEqual(event.getIntegerValueField(.keyboardEventKeycode), Int64(step.code))
                XCTAssertEqual(event.flags.intersection(KeyEventSequence.modifierFlags), step.flags)
                XCTAssertEqual(event.getIntegerValueField(.eventSourceUserData), KeyEventStep.sourceTag)
                if step.code == 76 || step.code == 123 { XCTAssertTrue(event.flags.contains(.maskNumericPad)) }
                if step.code == 122 || step.code == 123 { XCTAssertTrue(event.flags.contains(.maskSecondaryFn)) }
            }
        }
    }
}
