import AppKit
import SwiftUI
import ThinCubeCore

@MainActor final class ShortcutRecorder: ObservableObject {
    @Published private(set) var isRecording = false
    private var capture = ShortcutCapture()
    private var monitor: Any?
    private var observers: [NSObjectProtocol] = []
    private weak var window: NSWindow?
    private var completion: ((String) -> Void)?
    private var onStop: (() -> Void)?

    func start(onStop: @escaping () -> Void, completion: @escaping (String) -> Void) {
        stop()
        NSApp.activate(ignoringOtherApps: true)
        guard let window = NSApp.keyWindow ?? NSApp.mainWindow ?? NSApp.windows.first(where: {
            $0.isVisible && $0.canBecomeKey && $0.title == "ThinCube"
        }) else { return }
        window.makeKeyAndOrderFront(nil)
        self.window = window
        self.onStop = onStop
        self.completion = completion
        capture = ShortcutCapture(); isRecording = true
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .keyUp, .flagsChanged]) { [weak self] event in
            guard let self, self.isRecording else { return event }
            guard NSApp.isActive, NSApp.keyWindow === self.window else { self.stop(); return event }
            // Match Folotoy: Escape cancels; local menu equivalents are consumed.
            if event.type == .keyDown, event.keyCode == 0x35 {
                self.stop()
                return nil
            }
            let flags = event.cgEvent?.flags ?? CGEventFlags(rawValue: UInt64(event.modifierFlags.rawValue))
            let complete = self.capture.receive(type: event.type, code: event.keyCode, flags: flags,
                                                isRepeat: event.type == .keyDown && event.isARepeat)
            if let complete {
                let completion = self.completion
                self.stop()
                completion?(complete)
            }
            return nil
        }
        guard monitor != nil else { stop(); return }
        for (name, object) in [(NSWindow.didResignKeyNotification, window as AnyObject?),
                               (NSApplication.didResignActiveNotification, nil)] {
            observers.append(NotificationCenter.default.addObserver(forName: name, object: object, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.stop() }
            })
        }
    }

    func stop() {
        if let monitor { NSEvent.removeMonitor(monitor) }
        monitor = nil
        observers.forEach(NotificationCenter.default.removeObserver); observers.removeAll()
        let onStop = self.onStop
        self.onStop = nil; completion = nil; window = nil; isRecording = false
        onStop?()
    }

    deinit {
        if let monitor { NSEvent.removeMonitor(monitor) }
        observers.forEach(NotificationCenter.default.removeObserver)
    }
}


/// The shortcut editor uses the same colours and dimensions as Folotoy.
private enum ShortcutStyle {
    static let amber = Color(red: 0.973, green: 0.690, blue: 0.000)
    static let amberHi = Color(red: 1.000, green: 0.788, blue: 0.302)
    static let groupFill = Color.white.opacity(0.05)
}

struct ShortcutEditor: View {
    @ObservedObject var model: DeviceModel
    @StateObject private var recorder = ShortcutRecorder()
    @State private var choosing = false
    @State private var hint = "可录制组合键，或点击「选择快捷键」直接设置。"

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            buttonRow
                .background(ShortcutStyle.groupFill, in: RoundedRectangle(cornerRadius: 10))

            Text(hint)
                .font(.system(size: 11.5))
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            HStack(alignment: .center, spacing: 12) {
                VStack(alignment: .leading, spacing: 3) {
                    Text("区分左右修饰键").font(.system(size: 13))
                    Text("统一用于录制和选择。开启后，可分别设置左、右 Command / Shift / Option / Control。")
                        .font(.system(size: 11.5)).foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 12)
                Toggle("区分左右修饰键", isOn: Binding(
                    get: { model.shortcut.distinguishSides },
                    set: { on in
                        recorder.stop()
                        var shortcut = model.shortcut
                        shortcut.distinguishSides = on
                        // Generic modifiers already inject the left key. Make
                        // that choice explicit when enabling side selection.
                        if on, let explicit = ShortcutSelection(shortcut: shortcut).shortcut { shortcut = explicit }
                        if model.setShortcut(shortcut) {
                            hint = on ? "已区分左右修饰键；录制和选择均按此设置生效。" : "已合并左右修饰键；原侧别会保留，重新开启可恢复。"
                        } else { hint = model.note }
                    }
                ))
                .toggleStyle(.switch)
                .labelsHidden()
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 10)
            .background(ShortcutStyle.groupFill, in: RoundedRectangle(cornerRadius: 10))
        }
        .sheet(isPresented: $choosing) {
            ShortcutChooser(shortcut: model.shortcut) { shortcut in
                guard model.setShortcut(shortcut) else { return model.note }
                hint = "已设置 \(shortcut.label)。"
                return nil
            }
            .onAppear { model.setShortcutRecording(true) }
            .onDisappear { model.setShortcutRecording(false) }
        }
        .onDisappear { recorder.stop(); choosing = false }
        .onChange(of: model.updating) { if $0 { recorder.stop(); choosing = false } }
        .onChange(of: model.connecting) { if $0 { recorder.stop(); choosing = false } }
        .onChange(of: model.name) { _ in
            recorder.stop(); choosing = false
            hint = "可录制组合键，或点击「选择快捷键」直接设置。"
        }
    }

    private var buttonRow: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 1) {
                Text("正面 A 键").font(.system(size: 13))
                Text("A").font(.system(size: 10.5)).foregroundStyle(.secondary)
            }
            .frame(width: 72, alignment: .leading)

            if recorder.isRecording {
                Text("等待按键…")
                    .font(.system(size: 13))
                    .foregroundStyle(ShortcutStyle.amberHi)
            } else {
                ScrollView(.horizontal, showsIndicators: false) {
                    KeycapRow(spec: model.shortcut.effectiveSpec)
                }
                .frame(minWidth: 30)
            }
            Spacer(minLength: 8)
            Button("选择快捷键…") {
                recorder.stop()
                choosing = true
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            Button(recorder.isRecording ? "停止录制" : "录制") {
                if recorder.isRecording { recorder.stop() }
                else {
                    hint = "请按下所需组合键，松开全部按键即保存；Esc 取消。"
                    model.setShortcutRecording(true)
                    recorder.start(onStop: { model.setShortcutRecording(false) }) { applyRecorded($0) }
                    if !recorder.isRecording { model.setShortcutRecording(false) }
                }
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .frame(width: 76)
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 54)
    }

    private func applyRecorded(_ candidate: String) {
        // Retain the physical sides even while the shared side setting is off.
        let shortcut = Shortcut(spec: candidate, distinguishSides: model.shortcut.distinguishSides)
        if model.setShortcut(shortcut) { hint = "已录制 \(shortcut.label)。" }
        else { hint = model.note }
    }
}

private struct ShortcutChooser: View {
    @Environment(\.dismiss) private var dismiss
    @State private var draft: ShortcutSelection
    @State private var error: String?
    let apply: (Shortcut) -> String?

    init(shortcut: Shortcut, apply: @escaping (Shortcut) -> String?) {
        _draft = State(initialValue: ShortcutSelection(shortcut: shortcut))
        self.apply = apply
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 6) {
                Text("选择快捷键").font(.system(size: 20, weight: .semibold))
                Text("直接选择，无需实际按键。只使用修饰键时，将主键设为「无」。")
                    .font(.system(size: 12)).foregroundStyle(.secondary)
            }

            VStack(alignment: .leading, spacing: 12) {
                Text("修饰键").font(.system(size: 13, weight: .medium))
                if draft.distinguishSides {
                    ForEach(["ctrl", "opt", "shift", "cmd"], id: \.self) { group in
                        HStack {
                            Text(modifierName(group)).frame(width: 110, alignment: .leading)
                            ForEach(ShortcutCatalog.modifiers.filter { $0.group == group }, id: \.code) { modifier in
                                modifierToggle(modifier, label: modifier.label)
                                    .frame(maxWidth: .infinity, alignment: .leading)
                            }
                        }
                    }
                } else {
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], alignment: .leading, spacing: 12) {
                        ForEach(ShortcutCatalog.modifiers.filter { $0.token.hasPrefix("left_") }, id: \.code) { modifier in
                            modifierToggle(modifier, label: "\(modifierName(modifier.group)) \(ShortcutCatalog.label(modifier.group))")
                        }
                    }
                }
                Text(draft.distinguishSides ? "已开启区分左右键，可同时选择左右两侧。" : "左右修饰键合并；可在按键页开启区分左右键。")
                    .font(.system(size: 11.5)).foregroundStyle(.secondary)
            }
            .padding(14)
            .background(ShortcutStyle.groupFill, in: RoundedRectangle(cornerRadius: 10))

            Picker("主键", selection: $draft.keyCode) {
                Text("无（仅修饰键）").tag(UInt16?.none)
                ForEach(keyGroups, id: \.title) { group in
                    Section(group.title) {
                        ForEach(group.keys, id: \.code) { key in
                            Text(key.label).tag(Optional(key.code))
                        }
                    }
                }
                if let code = draft.keyCode, !ShortcutCatalog.keys.contains(where: { $0.code == code }) {
                    Text("键 \(code)").tag(Optional(code))
                }
            }
            .pickerStyle(.menu)

            HStack {
                Text("快捷键预览").font(.system(size: 12)).foregroundStyle(.secondary)
                if let shortcut = draft.shortcut {
                    ScrollView(.horizontal, showsIndicators: false) { KeycapRow(spec: shortcut.effectiveSpec) }
                        .frame(height: 32)
                }
                else { Text("请至少选择一个按键").font(.system(size: 12)).foregroundStyle(.secondary) }
            }
            .frame(minHeight: 32)
            if let error { Text(error).font(.system(size: 12)).foregroundStyle(.red) }
            Divider()
            HStack {
                Spacer()
                Button("取消") { dismiss() }.keyboardShortcut(.cancelAction)
                Button("使用快捷键") {
                    guard let shortcut = draft.shortcut else { return }
                    error = apply(shortcut)
                    if error == nil { dismiss() }
                }
                .keyboardShortcut(.defaultAction)
                .disabled(draft.shortcut == nil)
            }
        }
        .padding(24)
        .frame(width: 540)
    }

    private func modifierToggle(_ modifier: ShortcutCatalog.Modifier, label: String) -> some View {
        Toggle(label, isOn: Binding(
            get: { draft.modifierCodes.contains(modifier.code) },
            set: { on in
                if on { draft.modifierCodes.insert(modifier.code) }
                else { draft.modifierCodes.remove(modifier.code) }
            }
        ))
        .toggleStyle(.checkbox)
        .accessibilityLabel(draft.distinguishSides ? "\(modifier.label) \(modifierName(modifier.group))" : label)
    }

    private func modifierName(_ group: String) -> String {
        ["ctrl": "Control", "opt": "Option", "shift": "Shift", "cmd": "Command"][group] ?? group
    }

    private var keyGroups: [(title: String, keys: [ShortcutCatalog.Key])] {
        let definitions: [(String, [String])] = [
            ("常用键", ["space", "return", "tab", "escape", "delete", "forward_delete"]),
            ("字母", Array("abcdefghijklmnopqrstuvwxyz").map(String.init)),
            ("数字", (0...9).map(String.init)),
            ("符号", ["grave", "minus", "equal", "left_bracket", "right_bracket", "backslash", "semicolon", "quote", "comma", "period", "slash", "iso_section"]),
            ("方向与导航", ["up", "down", "left", "right", "home", "end", "page_up", "page_down", "help"]),
            ("功能键", (1...20).map { "f\($0)" }),
            ("小键盘", (0...9).map { "keypad_\($0)" } + ["keypad_decimal", "keypad_multiply", "keypad_plus", "keypad_minus", "keypad_divide", "keypad_equal", "keypad_enter", "keypad_clear"]),
        ]
        return definitions.map { title, tokens in
            (title, tokens.compactMap { token in ShortcutCatalog.keys.first { $0.token == token } })
        }
    }
}

/// Shortcut spec rendered as key caps ("⌘" "Z").
struct KeycapRow: View {
    let spec: String
    var body: some View {
        HStack(spacing: 5) {
            ForEach(Array(tokens.enumerated()), id: \.offset) { _, token in
                Text(token)
                    .font(.system(size: 13, weight: .semibold))
                    .padding(.horizontal, 8)
                    .frame(minWidth: 28, minHeight: 26)
                    .background(
                        LinearGradient(colors: [.white.opacity(0.13), .white.opacity(0.05)],
                                       startPoint: .top, endPoint: .bottom),
                        in: RoundedRectangle(cornerRadius: 6)
                    )
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(.white.opacity(0.09), lineWidth: 0.5))
            }
        }
    }

    private var tokens: [String] {
        guard ShortcutCatalog.parse(spec) != nil else { return ["未设置"] }
        return ShortcutCatalog.label(spec).components(separatedBy: " + ")
    }
}
