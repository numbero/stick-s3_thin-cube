import AppKit
import ApplicationServices
import ThinCubeCore

@MainActor final class KeyInjector {
    private var sequence = KeyEventSequence()
    // Track our synthetic source independently from other applications.
    private let source = CGEventSource(stateID: .privateState)
    private let targetPID: pid_t?
    var onTrace: ((String) -> Void)?
    var isActive: Bool { sequence.isActive }

    init(targetPID: pid_t? = nil) { self.targetPID = targetPID }

    // Test the capability used by CGEventPost itself. An Accessibility UI
    // checkbox (or a cached AX trust result) is not proof that posting works.
    var trusted: Bool { CGPreflightPostEventAccess() }

    func requestPermission() {
        let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
        _ = AXIsProcessTrustedWithOptions(options)
    }

    @discardableResult func press(_ shortcut: Shortcut, button: DeviceButton = .a) -> Bool {
        guard trusted else { onTrace?("拦截：系统键盘控制权限未生效；请在辅助功能中允许当前 ThinCube 应用。"); return false }
        guard let combo = shortcut.combo else { onTrace?("拦截：快捷键配置无效。"); return false }
        guard !sequence.isActive(for: button) else { onTrace?("忽略：该设备按键的快捷键仍保持中。"); return false }
        let physical = physicalState(checking: combo.allCodes)
        if let code = combo.keyCode, physical.keys.contains(code), !sequence.injectedCodes.contains(code) {
            onTrace?("拦截：主键 code=\(code) 已被物理键盘按住；请先松开。"); return false
        }
        var next = sequence
        guard let steps = next.press(combo, physicallyHeld: physical.keys,
                                     physicalFlags: physical.flags, button: button),
              let events = makeEvents(steps) else { onTrace?("失败：无法构造系统键盘事件。"); return false }
        post(events)
        sequence = next
        trace(steps)
        if steps.isEmpty { onTrace?("未提交新事件：修饰键已被物理键盘按住。") }
        return true
    }

    func release(_ button: DeviceButton? = nil) {
        guard sequence.isActive else { return }
        let physical = physicalState(checking: [])
        var next = sequence
        let steps = next.release(physicallyHeld: physical.keys, physicalFlags: physical.flags, button: button)
        guard let events = makeEvents(steps) else { onTrace?("失败：无法构造松键事件，将继续尝试释放。"); return }
        post(events)
        sequence = next
        trace(steps)
    }

    private func trace(_ steps: [KeyEventStep]) {
        let target = targetPID == nil ? (NSWorkspace.shared.frontmostApplication?.localizedName ?? "未知应用") : "ThinCube（本窗口测试）"
        for step in steps {
            let key = ShortcutCatalog.label(ShortcutCatalog.modifier(step.code)?.token ?? "keycode_\(step.code)")
            let type = step.isModifier ? "flagsChanged" : (step.down ? "keyDown" : "keyUp")
            onTrace?("已提交 \(step.down ? "按下" : "松开") \(key) · \(type) · code=\(step.code) · flags=0x\(String(step.flags.rawValue, radix: 16)) · 目标=\(target)")
        }
    }

    private func post(_ events: [CGEvent]) {
        for event in events {
            if let targetPID { event.postToPid(targetPID) }
            else { event.post(tap: .cghidEventTap) }
        }
    }

    private func makeEvents(_ steps: [KeyEventStep]) -> [CGEvent]? {
        var events: [CGEvent] = []
        for step in steps {
            guard let event = step.makeEvent(source: source) else { return nil }
            events.append(event)
        }
        return events
    }

    private func physicalState(checking codes: [UInt16]) -> (keys: Set<UInt16>, flags: CGEventFlags) {
        // The planner excludes owned synthetic keys during release: these
        // queries must not be assumed to contain only physical keyboard state.
        let checked = Set(codes + ShortcutCatalog.modifiers.map(\.code))
        let held = Set(checked.filter { CGEventSource.keyState(.hidSystemState, key: $0) })
        let flags = CGEventSource.flagsState(.hidSystemState)
            .intersection([.maskAlphaShift, .maskSecondaryFn])
        return (held, flags)
    }
}
