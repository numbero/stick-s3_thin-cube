import XCTest
@testable import ThinCubeCore

final class TapUserSettingsTests: XCTestCase {
    func testRecommendedChoicesMatchCalibratedDefaultsExactly() {
        let defaults = TapConfiguration()
        XCTAssertEqual(defaults.strength, .normal)
        XCTAssertEqual(defaults.rhythm, .natural)
        XCTAssertEqual(defaults.protection, .standard)
        var selected = defaults
        selected.setStrength(.normal); selected.setRhythm(.natural); selected.setProtection(.standard)
        XCTAssertEqual(selected, defaults)
    }

    func testAllEverydayCombinationsAreValidAndBounded() {
        for strength in TapStrength.allCases {
            for rhythm in TapRhythm.allCases {
                for protection in TapProtection.allCases {
                    var config = TapConfiguration()
                    config.setStrength(strength); config.setRhythm(rhythm); config.setProtection(protection)
                    XCTAssertTrue(config.isValid)
                    XCTAssertTrue((450...700).contains(config.softThresholdMg))
                    XCTAssertTrue((350...550).contains(config.softWindowMs))
                    XCTAssertEqual(config.strength, strength)
                    XCTAssertEqual(config.rhythm, rhythm)
                    XCTAssertEqual(config.protection, protection)
                }
            }
        }
    }

    func testChoicesPreserveOtherAdvancedFieldsAndDetectCustomValues() {
        var config = TapConfiguration()
        config.axis = 1; config.sensitivity = 3; config.softReboundMs = 210
        config.softGyroFilterMs = 120; config.enabled = false
        var expected = config
        expected.softThresholdMg = 450
        config.setStrength(.light)
        XCTAssertEqual(config, expected)
        expected.softWindowMs = 550
        config.setRhythm(.relaxed)
        XCTAssertEqual(config, expected)
        config.setProtection(.stronger)
        expected.softGyroDps = 150; expected.softMotionGuardMs = 450
        expected.settleMs = 700; expected.buttonGuardMs = 700; expected.cooldownMs = 800
        XCTAssertEqual(config, expected)
        config.softThresholdMg = 555; config.softWindowMs = 455; config.cooldownMs = 801
        XCTAssertNil(config.strength); XCTAssertNil(config.rhythm); XCTAssertNil(config.protection)
        XCTAssertTrue(config.usesEverydayDetector)
        config.manual = true; XCTAssertFalse(config.usesEverydayDetector)
        config.manual = false; config.software = false; XCTAssertFalse(config.usesEverydayDetector)
    }
}
