import SwiftUI
import ThinCubeCore

@MainActor final class TapSettingsModel: ObservableObject {
    @Published private(set) var snapshot: TapTelemetry?
    @Published var draft = TapConfiguration() {
        didSet {
            guard !working, draft != oldValue else { return }
            saveError = nil
            if hasChanges {
                message = "调整尚未保存，点击「保存到设备」生效。"; failed = false
            } else if fresh {
                message = "与设备当前设置一致。"; failed = false
            }
        }
    }
    @Published private(set) var message = "正在读取设备设置…"
    @Published private(set) var working = false
    @Published private(set) var fresh = false
    @Published private(set) var failed = false
    private var operation = 0
    private var saveError: String?

    var hasChanges: Bool { snapshot?.config.map { $0 != draft } ?? false }

    private func read(_ model: DeviceModel) async throws -> TapTelemetry {
        let value = try await model.tapRequest()
        let result = try JSONDecoder().decode(TapTelemetry.self, from: JSONSerialization.data(withJSONObject: value))
        guard !Task.isCancelled else { throw CancellationError() }
        return result
    }

    func monitor(_ model: DeviceModel) async {
        snapshot = nil; fresh = false; failed = false; saveError = nil
        message = "正在读取设备设置…"
        var first = true
        while !Task.isCancelled {
            if !working {
                let started = operation
                do {
                    let result = try await read(model)
                    // A read started before Apply must not replace its newer acknowledgement.
                    guard started == operation, !working else { continue }
                    let wasSaved = snapshot?.saved
                    snapshot = result; fresh = true
                    if !result.supported {
                        failed = true; message = "此固件不支持敲击设置，请先升级设备固件。"; return
                    }
                    guard result.config != nil else { throw ClientError.message("设备未返回完整设置。") }
                    let initial = first
                    if first, let config = result.config {
                        draft = config; first = false
                    }
                    if !result.supportsPersistence {
                        message = "当前固件仅支持临时设置，请升级到 v0.4.57 或更新版本后保存。"; failed = true
                    } else if result.storage == "invalid" {
                        message = "设备中保存的设置校验未通过，敲击检测已暂停。请重试保存或主动恢复推荐。"; failed = true
                    } else if result.storage == "unavailable" {
                        message = "设备存储暂时无法确认，请重试保存或主动恢复推荐。"; failed = true
                    } else {
                        if let saveError { message = saveError }
                        else if initial { message = result.saved == true ? "已读取设备保存的设置。" : "当前设置尚未保存，点击「保存到设备」后重启仍会保留。" }
                        else if wasSaved != result.saved && !hasChanges {
                            message = result.saved == true ? "设备已确认设置已保存，重启后仍会保留。" : "当前设置尚未保存到设备。"
                        }
                        else if failed { message = "已重新读取设备设置，未保存的调整已保留。" }
                        failed = saveError != nil
                    }
                } catch {
                    if Task.isCancelled { return }
                    guard started == operation, !working else { continue }
                    fresh = false; failed = true
                    message = "暂时无法读取设置：\(error.localizedDescription)"
                }
            }
            do { try await Task.sleep(nanoseconds: 500_000_000) } catch { return }
        }
    }

    func apply(_ model: DeviceModel, resetCounters: Bool = false, restoreRecommended: Bool = false) async {
        guard !working, fresh, snapshot?.supported == true,
              resetCounters || snapshot?.supportsPersistence == true,
              resetCounters || restoreRecommended || draft.isValid else { return }
        working = true; failed = false; saveError = nil; operation += 1
        message = resetCounters ? "正在清零计数…" : "正在保存并确认设备设置…"
        defer { working = false }
        let id = UUID().uuidString
        var requested = draft
        if restoreRecommended {
            requested = TapConfiguration(); requested.enabled = snapshot?.config?.enabled ?? draft.enabled
        }
        do {
            var body: [String: Any] = ["action": resetCounters ? "reset" : restoreRecommended ? "restore" : "configure", "requestId": id]
            if !resetCounters && !restoreRecommended { body["config"] = try JSONSerialization.jsonObject(with: JSONEncoder().encode(requested)) }
            let response = try await model.tapRequest(body)
            if let error = response["error"] as? String { throw ClientError.message(error) }
            guard response["accepted"] as? Bool == true else { throw ClientError.message("设备未接受请求。") }
            for _ in 0..<12 {
                let result = try await read(model)
                snapshot = result; fresh = true
                if result.requestId == id {
                    guard result.confirms(id, configuration: resetCounters ? nil : requested, requireSaved: !resetCounters) else {
                        throw ClientError.message(saveFailure(result.result))
                    }
                    if !resetCounters, let config = result.config { draft = config }
                    message = resetCounters ? "识别计数已清零，保存的设置保持不变。" : restoreRecommended ? "已恢复推荐并保存到设备，重启后仍会保留。" : "设置已保存到设备，重启后仍会保留。"
                    return
                }
                try await Task.sleep(nanoseconds: 150_000_000)
            }
            throw ClientError.message("等待设备确认超时，请检查连接后重试。")
        } catch {
            fresh = false; failed = true
            message = error.localizedDescription; saveError = message
        }
    }
    private func saveFailure(_ result: String?) -> String {
        switch result {
        case "storage_failed_restored": return "保存失败，已恢复之前的设备设置，请重试。"
        case "storage_rollback_failed": return "设备存储未能恢复确认，请重新连接后检查设置。"
        case "write_failed_restored": return "设置未能生效，已恢复之前的参数。"
        case "readback_failed": return "传感器未能确认设置，敲击检测已暂停，请重连设备后重试。"
        case "busy": return "设备暂时忙，请稍后重试。"
        default: return "设备尚未确认设置已保存，请重新读取后重试。"
        }
    }
}

struct TapSettingsPanel: View {
    @ObservedObject var model: DeviceModel
    @StateObject private var settings = TapSettingsModel()
    @State private var showAdvanced = false
    @State private var confirmRestore = false
    @Environment(\.dismiss) private var dismiss

    private var editable: Bool {
        settings.fresh && settings.snapshot?.supported == true && settings.snapshot?.supportsPersistence == true && !settings.working &&
        model.connected && !model.updating && !model.switching
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                VStack(alignment: .leading, spacing: 5) {
                    Text("敲击设置").font(.system(size: 20, weight: .semibold))
                    Text("让双敲更贴合你的使用习惯。")
                        .font(.callout).foregroundStyle(.secondary)
                }
                Spacer()
                HelpPopover(title: "敲击设置帮助") {
                    HelpParagraph("从推荐值开始", "力度选适中、节奏选自然、防误触选标准，即之前校正的默认组合。更换档位后点击保存到设备。")
                    HelpParagraph("按习惯调整", "漏掉轻敲时可降低所需力度；两下敲得较慢可放宽节奏；日常换握容易触发时可加强防误触。加强防护也可能拦截晃动中的有意双敲。")
                    HelpParagraph("保存在设备", "设置直接保存到设备，重启和重新连接后自动加载，不依赖 Mac 重发。此窗口打开时不发送双敲快捷键，关闭后在倒置对讲模式下生效。")
                    HelpParagraph("恢复推荐", "主动确认恢复后，推荐参数会保存到设备，当前启用开关保持不变。清零识别计数不会修改设置。")
                    HelpParagraph("高级设置", "保留完整底层参数和诊断计数。自定义数值不会被自动取整到日常档位；未保存的调整可以撤销。")
                }
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    enableCard
                    everydayCard
                    statusCard
                    advancedCard
                }.padding(.vertical, 1)
            }
            VStack(alignment: .leading, spacing: 10) {
                if !settings.draft.isValid {
                    Text("有参数超出可用范围，请展开高级设置检查。")
                        .font(.caption).foregroundStyle(.orange)
                }
                Text(settings.message)
                    .font(.system(size: 11.5)).foregroundStyle(settings.failed ? .orange : .secondary)
                    .fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
                HStack(alignment: .center, spacing: 12) {
                    Text(storageNote)
                        .font(.system(size: 10.5)).foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                    Spacer(minLength: 0)
                    Button("完成") { dismiss() }
                        .keyboardShortcut(.cancelAction).disabled(settings.working)
                    Button(settings.working ? "处理中…" : "保存到设备") { Task { await settings.apply(model) } }
                        .keyboardShortcut(.defaultAction)
                        .disabled(!editable || !settings.draft.isValid || (!settings.hasChanges && settings.snapshot?.verified == true && settings.snapshot?.saved == true))
                }
            }
        }
        .padding(22).frame(width: 620, height: 710)
        .tint(ShortcutStyle.amber)
        .interactiveDismissDisabled(settings.working)
        .confirmationDialog("恢复推荐并保存到设备？", isPresented: $confirmRestore) {
            Button("恢复并保存") { Task { await settings.apply(model, restoreRecommended: true) } }
            Button("取消", role: .cancel) {}
        } message: {
            Text("力度、节奏、防误触和高级参数将恢复为推荐值。设备当前的开启状态保持不变，恢复后重启也会保留。")
        }
        .onAppear { model.setTapTuning(true) }
        .onDisappear { model.setTapTuning(false) }
        .task(id: "\(model.name)-\(model.connected)-\(model.currentTransport)-\(model.updating)-\(model.switching)") {
            await settings.monitor(model)
        }
    }

    private var enableCard: some View {
        HStack(spacing: 14) {
            VStack(alignment: .leading, spacing: 4) {
                Text("开启双敲").font(.system(size: 13))
                Text(settings.draft.enabled ? "倒置对讲时，双敲正面面板执行快捷键。" : "双敲已关闭，参数保留；重新开启后可调整。")
                    .font(.system(size: 11.5)).foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            Spacer()
            Toggle("开启双敲", isOn: Binding(get: { settings.draft.enabled }, set: {
                settings.draft.enabled = $0
                if $0 { settings.draft.doubleTap = true }
            }))
            .toggleStyle(.switch).labelsHidden().disabled(!editable)
        }.tapSettingsCard()
    }

    private var everydayCard: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Text("日常调节").font(.system(size: 13, weight: .semibold))
                Spacer()
                Button("恢复推荐…") { confirmRestore = true }
                    .buttonStyle(.bordered).controlSize(.small).disabled(!editable)
            }
            if !settings.draft.usesEverydayDetector {
                Text("当前使用高级调试模式。恢复推荐设置后即可使用日常调节。")
                    .font(.callout).foregroundStyle(.orange)
                    .fixedSize(horizontal: false, vertical: true)
            }
            VStack(alignment: .leading, spacing: 16) {
                choiceRow("敲击力度", detail: strengthDetail) {
                    Picker("敲击力度", selection: Binding<TapStrength?>(get: { settings.draft.strength }, set: {
                        if let value = $0 { settings.draft.setStrength(value) }
                    })) {
                        if settings.draft.strength == nil { Text("自定义").tag(Optional<TapStrength>.none) }
                        Text("轻柔").tag(Optional(TapStrength.light))
                        Text("适中").tag(Optional(TapStrength.normal))
                        Text("用力").tag(Optional(TapStrength.firm))
                    }
                }
                choiceRow("双敲节奏", detail: rhythmDetail) {
                    Picker("双敲节奏", selection: Binding<TapRhythm?>(get: { settings.draft.rhythm }, set: {
                        if let value = $0 { settings.draft.setRhythm(value) }
                    })) {
                        if settings.draft.rhythm == nil { Text("自定义").tag(Optional<TapRhythm>.none) }
                        Text("快速").tag(Optional(TapRhythm.quick))
                        Text("自然").tag(Optional(TapRhythm.natural))
                        Text("稍慢").tag(Optional(TapRhythm.relaxed))
                    }
                }
                choiceRow("防误触", detail: protectionDetail) {
                    Picker("防误触", selection: Binding<TapProtection?>(get: { settings.draft.protection }, set: {
                        if let value = $0 { settings.draft.setProtection(value) }
                    })) {
                        if settings.draft.protection == nil { Text("自定义").tag(Optional<TapProtection>.none) }
                        Text("标准").tag(Optional(TapProtection.standard))
                        Text("加强").tag(Optional(TapProtection.stronger))
                    }
                }
            }.disabled(!editable || !settings.draft.enabled || !settings.draft.usesEverydayDetector)
        }.tapSettingsCard()
    }

    private var storageNote: String {
        guard settings.snapshot?.supportsPersistence == true else { return "需要支持设备保存的新固件" }
        if settings.hasChanges { return "有未保存的调整 · 保存后重启仍保留" }
        return settings.snapshot?.saved == true ? "已保存到设备 · 重启后保留" : "当前设置尚未保存到设备"
    }

    private var statusCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Label(statusLabel, systemImage: "circle.fill")
                    .font(.system(size: 11.5)).foregroundStyle(statusColor)
                Spacer()
                if let s = settings.snapshot, s.supported {
                    Text("累计识别 \((s.config?.software == true ? s.softwareRaw : s.raw) ?? 0) 次双敲")
                        .font(.system(size: 11.5)).foregroundStyle(.secondary)
                }
            }
            Text("设置期间不触发快捷键；关闭窗口后在对讲模式下生效。")
                .font(.system(size: 11.5)).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }.tapSettingsCard()
    }

    private var advancedCard: some View {
        DisclosureGroup(isExpanded: $showAdvanced) {
            if showAdvanced {
                TapAdvancedSettings(configuration: $settings.draft, snapshot: settings.snapshot,
                                    fresh: settings.fresh, editable: editable) {
                    Task { await settings.apply(model, resetCounters: true) }
                }.padding(.top, 14)
            }
        } label: {
            VStack(alignment: .leading, spacing: 4) {
                Text("高级设置").font(.system(size: 13))
                Text("完整参数与诊断信息，通常无需调整。")
                    .font(.system(size: 11.5)).foregroundStyle(.secondary)
            }
        }
        .tapSettingsCard()
    }

    private func choiceRow<Content: View>(_ title: String, detail: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title).font(.system(size: 12.5))
            content().pickerStyle(.segmented).labelsHidden()
            Text(detail).font(.system(size: 11)).foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var strengthDetail: String {
        switch settings.draft.strength {
        case .light: return "轻敲也容易触发；若偶尔误触，可改为适中。"
        case .normal: return "推荐起点，沿用之前校正的敲击力度。"
        case .firm: return "需要更明确的敲击，减少轻微碰撞带来的触发。"
        case nil: return "正在使用自定义力度，选择档位可替换这项设置。"
        }
    }
    private var rhythmDetail: String {
        switch settings.draft.rhythm {
        case .quick: return "适合快速连敲，识别后的等待更短。"
        case .natural: return "推荐起点，按自然节奏连续敲两下。"
        case .relaxed: return "允许两下之间稍慢，响应也会稍晚。"
        case nil: return "正在使用自定义节奏，选择档位可替换这项设置。"
        }
    }
    private var protectionDetail: String {
        switch settings.draft.protection {
        case .standard: return "推荐起点，平衡日常动作过滤与双敲响应。"
        case .stronger: return "加强换握、晃动和按键后的过滤，也可能漏掉晃动中的双敲。"
        case nil: return "正在使用自定义防护，选择档位会替换相关过滤设置。"
        }
    }
    private var statusLabel: String {
        guard settings.fresh, let s = settings.snapshot else { return "等待设备连接" }
        guard s.supported else { return "需要升级固件" }
        guard s.verified == true, s.healthy == true else { return "检测状态异常" }
        guard s.enabled == true else { return "双敲已关闭" }
        if s.config?.manual == true { return "实验模式：仅记录" }
        if s.config?.software == true, s.softwareFifo != true {
            return s.context == false ? "双敲待命" : "检测尚未就绪"
        }
        return "检测正常"
    }
    private var statusColor: Color {
        statusLabel == "检测正常" ? .green : ["双敲已关闭", "双敲待命"].contains(statusLabel) ? .secondary : .orange
    }
}

private extension View {
    func tapSettingsCard() -> some View {
        padding(14).frame(maxWidth: .infinity, alignment: .leading)
            .background(ShortcutStyle.groupFill, in: RoundedRectangle(cornerRadius: ShortcutStyle.groupCornerRadius))
    }
}

private struct TapAdvancedSettings: View {
    @Binding var configuration: TapConfiguration
    let snapshot: TapTelemetry?
    let fresh: Bool
    let editable: Bool
    let clearCounters: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            section("识别方式") {
                Toggle("使用软件双敲识别", isOn: $configuration.software)
                Text("关闭后改用芯片原生报告，日常档位将不可用。")
                    .font(.caption).foregroundStyle(.secondary)
                Toggle("手持实验模式（始终只记录）", isOn: $configuration.manual)
            }.disabled(!configuration.enabled)
            section("软件识别参数") {
                number("敲击阈值 · mg", value: $configuration.softThresholdMg, range: 100...2000)
                number("回弹排除 · ms", value: $configuration.softReboundMs, range: 80...300)
                number("早到冲击相对强度 · %", value: $configuration.softRatioPercent, range: 40...95)
                number("双敲窗口 · ms", value: $configuration.softWindowMs, range: 250...1000)
                number("旋转抑制阈值 · °/s", value: $configuration.softGyroDps, range: 30...600)
                number("旋转滤波时间 · ms", value: $configuration.softGyroFilterMs, range: 0...1000)
                number("旋转后抑制 · ms", value: $configuration.softMotionGuardMs, range: 100...2000)
                number("重力基线滤波 · ms", value: $configuration.softBaselineMs, range: 50...1000)
            }.disabled(!configuration.enabled || !configuration.software)
            section("动作过滤") {
                number("进入场景后稳定 · ms", value: $configuration.settleMs, range: 0...5000)
                number("按 A/B 后抑制 · ms", value: $configuration.buttonGuardMs, range: 0...5000)
                number("双敲后冷却 · ms", value: $configuration.cooldownMs, range: 0...10000)
            }.disabled(!configuration.enabled)
            section("芯片原生参数") {
                HStack {
                    Toggle("单敲", isOn: $configuration.single)
                    Toggle("双敲", isOn: $configuration.doubleTap)
                    Toggle("三敲", isOn: $configuration.triple)
                }
                Picker("检测轴", selection: $configuration.axis) {
                    Text("X").tag(0); Text("Y").tag(1); Text("Z").tag(2)
                }.pickerStyle(.segmented)
                Toggle("使用应用滤波数据", isOn: $configuration.filtered)
                number("灵敏度系数（越小越敏感）", value: $configuration.sensitivity, range: 0...15)
                number("连续敲击窗口 · ms", value: $configuration.windowMs, range: 250...1000, step: 5)
                number("两组手势间安静 · ms", value: $configuration.quietMs, range: 250...500, step: 5)
                Toggle("等待窗口结束后按次数分类", isOn: $configuration.wait)
                Text("加速度固定为 200 Hz / ±8g。原生单敲与三敲只统计，不触发快捷键；当前驱动未实现 tap_shock_dur、min_const_thres。")
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }.disabled(!configuration.enabled)
            HStack {
                Button("撤销未保存的调整") {
                    if let config = snapshot?.config { configuration = config }
                }
                .disabled(snapshot?.config == configuration)
                Spacer()
                Button("清零识别计数", action: clearCounters)
            }.buttonStyle(.bordered).controlSize(.small)
            if let s = snapshot, s.supported { diagnostics(s) }
        }.disabled(!editable)
    }

    private func section<Content: View>(_ title: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 9) {
            Text(title).font(.system(size: 12, weight: .semibold))
            Divider()
            content().font(.system(size: 12))
        }
    }
    private func number(_ title: String, value: Binding<Int>, range: ClosedRange<Int>, step: Int = 1) -> some View {
        HStack(spacing: 8) {
            Text(title).fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 8)
            TextField(title, value: value, format: .number)
                .labelsHidden().textFieldStyle(.roundedBorder).frame(width: 76).multilineTextAlignment(.trailing)
                .accessibilityLabel(title)
            Stepper(title, value: value, in: range, step: step).labelsHidden().accessibilityLabel(title + "调节")
        }
    }
    private func diagnostics(_ s: TapTelemetry) -> some View {
        section("诊断信息") {
            VStack(alignment: .leading, spacing: 7) {
                Text("\(fresh ? "在线" : "数据过期") · \(s.verified == true ? "设备已确认" : "参数未确认") · 场景\(s.context == true ? "允许" : "拦截")")
                Text("芯片双敲 \(s.raw ?? 0) · 接受 \(s.accepted ?? 0) · 拦截 \(s.blocked ?? 0)")
                Text("软件双敲 \(s.softwareRaw ?? 0) · 接受 \(s.softwareAccepted ?? 0) · 拦截 \(s.softwareBlocked ?? 0)")
                Text("候选 \(s.softwareCandidates ?? 0) · 排除 \(s.softwareRejected ?? 0) · FIFO \(s.softwareFifo == true ? "运行" : "停止") · 样本 \(s.softwareSamples ?? 0)")
                Text("单敲 \(s.single ?? 0) · 三敲 \(s.triple ?? 0) · 未分类 \(s.unknown ?? 0)")
                Text("场景拦截 \(s.inactive ?? 0) · 按键 \(s.buttons ?? 0) · 稳定抑制 \(s.settling ?? 0) · 冷却 \(s.cooldown ?? 0)")
                Text("软件排除：旋转 \(s.softwareMotionRejected ?? 0) · 仅一下 \(s.softwareSingleRejected ?? 0) · 多次冲击 \(s.softwareMultipleRejected ?? 0)")
                Text("冲击时最大原始旋转 \(Int(s.softwareMaxGyro ?? 0)) °/s · I²C 错误 \(s.errors ?? 0) · 最大轮询间隔 \(s.maxPollMs ?? 0) ms")
                Text("最近事件 #\(s.eventSequence ?? 0) · 设备时间 \(s.eventAt ?? 0) ms · 配置版本 \(s.revision ?? 0)")
                Text("最近处理：\(reason(s.reason))")
            }.font(.system(size: 10.5, design: .monospaced)).textSelection(.enabled)
        }
    }
    private func reason(_ value: String?) -> String {
        switch value {
        case "accepted": return "已接受"
        case "button": return "按键正在保持"
        case "settling": return "场景稳定或按键后抑制"
        case "cooldown": return "冷却中"
        default: return "场景未就绪或尚无事件"
        }
    }
}
