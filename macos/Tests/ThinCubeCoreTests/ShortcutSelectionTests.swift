import XCTest
@testable import ThinCubeCore

final class ShortcutSelectionTests: XCTestCase {
    func testEveryKeyAndModifierCanBeSelectedWithoutRecording() throws {
        for key in ShortcutCatalog.keys {
            for modifier in ShortcutCatalog.modifiers {
                let original = Shortcut(spec: "\(modifier.token)+\(key.token)", distinguishSides: true)
                let draft = ShortcutSelection(shortcut: original)
                XCTAssertEqual(try XCTUnwrap(draft.shortcut), original)
            }
        }
    }

    func testModifierOnlyAndBothSidesRemainSelectable() throws {
        var draft = ShortcutSelection(shortcut: Shortcut(spec: "right_cmd", distinguishSides: true))
        XCTAssertNil(draft.keyCode)
        XCTAssertEqual(draft.shortcut?.spec, "right_cmd")
        draft.modifierCodes.insert(55)
        XCTAssertEqual(draft.shortcut?.spec, "left_cmd+right_cmd")
        XCTAssertEqual(draft.shortcut?.combo?.allCodes, [55, 54])
    }

    func testGenericModeCollapsesSidesAndCanSelectEscape() {
        var draft = ShortcutSelection(shortcut: Shortcut(spec: "left_cmd+right_cmd+space", distinguishSides: false))
        XCTAssertEqual(draft.modifierCodes, [55])
        draft.keyCode = 53
        XCTAssertEqual(draft.shortcut?.spec, "cmd+escape")
        XCTAssertEqual(draft.shortcut?.distinguishSides, false)
    }

    func testGenericModifierBecomesAnExplicitLeftChoiceInSideMode() {
        let draft = ShortcutSelection(shortcut: Shortcut(spec: "cmd", distinguishSides: true))
        XCTAssertEqual(draft.shortcut?.spec, "left_cmd")
    }

    func testEmptyAndUnsupportedSelectionsCannotBeApplied() {
        var draft = ShortcutSelection(shortcut: Shortcut())
        draft.keyCode = nil
        draft.modifierCodes = []
        XCTAssertNil(draft.shortcut)
        for code: UInt16 in [57, 63, 128, 55] {
            draft.keyCode = code
            XCTAssertNil(draft.shortcut)
        }
        draft.keyCode = 49
        draft.modifierCodes = [0]
        XCTAssertNil(draft.shortcut)
    }

    func testUncataloguedRecordedKeySurvivesOpeningTheChooser() {
        let shortcut = Shortcut(spec: "right_opt+keycode_52", distinguishSides: true)
        XCTAssertEqual(ShortcutSelection(shortcut: shortcut).shortcut, shortcut)
    }
}
