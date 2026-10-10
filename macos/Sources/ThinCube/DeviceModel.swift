import AppKit
import Combine
import Foundation
import Network
import CryptoKit
import ThinCubeCore

enum ClientError: LocalizedError {
    case message(String), transient(String)
    case http(Int, String)
    var errorDescription: String? {
        switch self { case .message(let value), .transient(let value), .http(_, let value): return value }
    }
}
struct DeviceStatus: Decodable {
    typealias Edge = DeviceKeyEdge
    struct TransportTransaction: Decodable { let requestId: String, source: String, target: String, previous: String, phase: String }
    let deviceId: String?, transport: String?, transportSwitch: Bool?, audioRate: Int?, transportAudioReady: Bool?
    let transportTransaction: TransportTransaction?, transportError: String?, rejectedRequest: String?
    let bleMtu: Int?, bleAudioDropped: UInt32?
    struct Calibration: Decodable { let seq: UInt32; let op: String; let ok: Bool }
    let epoch: UInt32, allowed: Bool, muted: Bool, localSound: Bool, streaming: Bool, manual: Bool, held: Bool
    let micReady: Bool, inverted: Bool, calibrated: Bool, faceCalibrated: Bool, bootVerified: Bool
    let gain: Double, brightness: Int, level: Double, ota: String, session: String
    let soundVolume: Int?, soundMuted: Bool?, soundPlaying: Bool?, soundCount: UInt32?
    let edge: Edge?
    let pose: String?, voiceBlock: String?, calibration: Calibration?
    var poseDescription: String {
        switch pose {
        case "upright": return "正常竖持 · A 键在下"
        case "inverted": return "竖向倒立 · A 键在上"
        case "face_up": return "屏幕朝上平放"
        case "face_down": return "屏幕朝下平放"
        case "landscape": return "横置探索甲板"
        case "moving": return "移动中或姿态尚未稳定"
        default: return "当前固件未提供姿态状态"
        }
    }
    var voiceHint: String {
        if streaming { return held ? "正在传音 · 快捷键保持中" : "正在持续传音" }
        switch voiceBlock {
        case "disabled": return "语音输入已禁用"
        case "muted": return "已静音，麦克风停止采集"
        case "busy": return "设备正在配网或升级"
        case "disconnected": return "等待 Mac 音频连接就绪"
        case "calibration": return "请先完成竖向倒立校准"
        case "sleeping": return "Clawd 正在休息"
        case "microphone": return "麦克风正在准备"
        case "rearm": return "请屏幕朝上平放，再倒立以开始传音；也可手动开始"
        case "invert": return "待命：竖向倒立并稍作停稳即可传音"
        default: return "待命"
        }
    }
}
private final class UploadProgress: NSObject, URLSessionTaskDelegate {
    var changed: ((Double) -> Void)?
    func urlSession(_ session: URLSession, task: URLSessionTask, didSendBodyData bytesSent: Int64, totalBytesSent: Int64, totalBytesExpectedToSend: Int64) {
        if totalBytesExpectedToSend > 0 { changed?(Double(totalBytesSent)/Double(totalBytesExpectedToSend)) }
    }
}
@MainActor final class DeviceModel: ObservableObject {
    @Published var pane: MainPane = .device
    @Published var host = UserDefaults.standard.string(forKey: "lastHost") ?? ""
    @Published var pin = ""
    @Published private(set) var pairingRequired = false
    @Published private(set) var connectionMode = ConnectionMode(rawValue: UserDefaults.standard.string(forKey: "connectionMode") ?? "") ?? .automatic
    @Published var name = "ThinCube"
    @Published var version = ""
    @Published var note = "选择设备后点击连接；自动模式会跟随设备的连接方式。"
    @Published var connected = false
    @Published var connecting = false
    @Published private(set) var reconnecting = false
    @Published var audioPrepared = false
    @Published private(set) var switching = false
    @Published private(set) var transportPhase = ""
    var currentTransport: String { status?.transport ?? (host.hasPrefix("ble://") ? "ble" : "wifi") }
    var transportLabel: String { currentTransport == "ble" ? "蓝牙" : "Wi-Fi" }
    var canSwitchTransport: Bool { connected && !connecting && !updating && !switching && status?.transportSwitch == true && status?.bootVerified == true }
    var canSelectConnectionMode: Bool { !updating && !connecting && !switching }
    var selectedDeviceID: String { target.deviceID ?? "" }
    var discoveredDeviceIDs: [String] {
        var ids = Set(discovery.devices.map(\.id))
        if let id = target.deviceID { ids.insert(id) }
        return ids.sorted()
    }
    var canSubmitPairingCode: Bool { pairingRequired && PairingCode.isValid(pin) }
    @Published var updating = false
    @Published var updateProgress = 0.0
    @Published var calibrationBusy = false
    @Published var status: DeviceStatus?
    @Published private(set) var shortcut = Shortcut()
    @Published private(set) var shortcutRecording = false
    private var shortcutRecordingCount = 0
    @Published var wifiSSID = ""
    @Published var wifiPassword = ""
    @Published var gain = 1.0
    @Published var brightness = 128.0
    @Published var soundVolume = 35.0
    @Published var audioDiagnostics = "尚未接收音频"
    let keys = KeyInjector()
    let keyDiagnostics = KeyDiagnostics()
    let discovery = Discovery()
    @Published private(set) var shortcutTestRunning = false
    private let testKeys = KeyInjector(targetPID: ProcessInfo.processInfo.processIdentifier)
    private var shortcutTestTask: Task<Void, Never>?
    private var keyStream = DeviceKeyStream()
    private var lastKeyDecodeFailure = Date.distantPast
    private let buffer = JitterBuffer()
    private lazy var bridge = AudioBridge(buffer: buffer)
    private let receiver = DatagramReceiver()
    private let session: URLSession
    private var base: URL?, key = ""
    private var websocket: URLSessionWebSocketTask?, reader: Task<Void, Never>?, timer: Timer?
    private var observers: [NSObjectProtocol] = []
    private var epoch: UInt32 = 0, ack: UInt32 = 0
    private var lastStatus = Date.distantPast, lastHeartbeat = Date.distantPast
    private var sendingHeartbeat = false
    private var generation = UUID()
    private var stopLocally = false
    private var lastDiagnostics = Date.distantPast
    private var soundUpdateTask: Task<Void, Never>?
    private var pendingCalibration: (op: String, seq: UInt32, deadline: Date)?
    private var recovery = ConnectionRecovery(enabled: false)
    private var target = ConnectionTarget(host: "", deviceID: nil)
    private var reconnectTask: Task<Void, Never>?, connectionTask: Task<Void, Never>?
    private var discoveryObserver: AnyCancellable?
    private let pathMonitor = NWPathMonitor()
    private let bleAudio = BLEAudioDecoder()
    private var activeBLE = false, preferredTransport = "wifi", pathSatisfied = true
    private var transportIntent: (id: String, target: String, previous: String, started: Date)?
    private var commitInFlight = false, lastSwitchRetry = Date.distantPast
    private var cachedCredential: (name: String, key: String)?
    private var pairingContinuation: CheckedContinuation<String, Error>?
    private var modeBeforeSwitch: ConnectionMode?

    init() {
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = 5; config.timeoutIntervalForResource = 140
        session = URLSession(configuration: config)
        loadShortcut()
        keys.onTrace = { [weak self] detail in
            self?.keyDiagnostics.record("快捷键", detail)
            if detail.hasPrefix("拦截：") || detail.hasPrefix("失败：") { self?.note = detail }
        }
        testKeys.onTrace = { [weak self] in self?.keyDiagnostics.record("本地注入", $0) }
        keyDiagnostics.onStop = { [weak self] in self?.cancelShortcutTest() }
        target = ConnectionTarget(host: host, deviceID: UserDefaults.standard.string(forKey: "lastDeviceName"))
        recovery = ConnectionRecovery(enabled: !host.isEmpty && (UserDefaults.standard.object(forKey: "autoReconnect") as? Bool ?? true))
        let center = NSWorkspace.shared.notificationCenter
        for (notification, reason) in [(NSWorkspace.willSleepNotification, ConnectionRecovery.Suspension.sleep),
                                       (NSWorkspace.sessionDidResignActiveNotification, .inactiveSession)] {
            observers.append(center.addObserver(forName: notification, object: nil, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.suspendRecovery(reason, note: "Mac 已休眠或锁定；恢复后将自动连接。") }
            })
        }
        for (notification, reason) in [(NSWorkspace.didWakeNotification, ConnectionRecovery.Suspension.sleep),
                                       (NSWorkspace.sessionDidBecomeActiveNotification, .inactiveSession)] {
            observers.append(center.addObserver(forName: notification, object: nil, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.resumeRecovery(reason) }
            })
        }
        for (notification, locked) in [("com.apple.screenIsLocked", true), ("com.apple.screenIsUnlocked", false)] {
            observers.append(DistributedNotificationCenter.default().addObserver(forName: NSNotification.Name(notification), object: nil, queue: .main) { [weak self] _ in
                Task { @MainActor in
                    if locked { self?.suspendRecovery(.lock, note: "Mac 已锁定；解锁后将自动连接。") }
                    else { self?.resumeRecovery(.lock) }
                }
            })
        }
        observers.append(NotificationCenter.default.addObserver(forName: NSApplication.willTerminateNotification, object: nil, queue: .main) { [weak self] _ in
            MainActor.assumeIsolated { self?.shutdown() }
        })
        preferredTransport = host.hasPrefix("ble://") ? "ble" : "wifi"
        discovery.ble.onStatus = { [weak self] data in
            guard let self, self.activeBLE else { return }; _ = self.receive(data)
        }
        discovery.ble.onAudio = { [weak self] data in
            guard let self, self.activeBLE, self.connected, !self.stopLocally, !self.switching,
                  let status = self.status, status.streaming, !status.muted, status.allowed,
                  let cookie = Data(hex: status.session) else { return }
            for frame in self.bleAudio.decodeFrames(data, session: cookie) { self.buffer.accept(frame) }
        }
        discovery.ble.onDisconnect = { [weak self] reason in
            guard let self, self.activeBLE else { return }; self.connectionFailed(reason)
        }
        discoveryObserver = discovery.$devices.sink { [weak self] devices in
            Task { @MainActor in self?.discovered(devices) }
        }
        pathMonitor.pathUpdateHandler = { [weak self] path in
            Task { @MainActor in
                guard let self else { return }
                self.pathSatisfied = path.status == .satisfied
                if self.pathSatisfied || self.connectionMode != .wifi || self.transportIntent?.target == "ble" { self.resumeRecovery(.network) }
                else { self.suspendRecovery(.network, note: "网络暂不可用，恢复后将自动连接。") }
            }
        }
        pathMonitor.start(queue: DispatchQueue(label: "ThinCube.connection.path"))
        scheduleRecovery(immediate: true)
        HardwareCheck.startIfRequested(model: self)
    }
    // Explicit developer check: metadata only, no PCM, keys, PINs or session cookies.
    func hardwareSnapshot() -> [String: Any] {
        let stats = buffer.statistics()
        return ["connected": connected, "connecting": connecting, "switching": switching,
                "transport": currentTransport, "phase": status?.transportTransaction?.phase ?? "",
                "requestId": status?.transportTransaction?.requestId ?? "", "audioPrepared": audioPrepared,
                "streaming": status?.streaming ?? false, "muted": status?.muted ?? false,
                "statusAge": Date().timeIntervalSince(lastStatus), "received": stats.receivedFrames,
                "renderedSamples": stats.renderedSamples, "concealed": stats.concealedFrames,
                "waitingSamples": stats.waitingSamples, "peak": stats.peak,
                "bleMtu": status?.bleMtu ?? 0, "bleAudioDropped": status?.bleAudioDropped ?? 0,
                "brightness": status?.brightness ?? 0, "calibrated": status?.calibrated ?? false,
                "faceCalibrated": status?.faceCalibrated ?? false, "keysActive": keys.isActive,
                "note": note]
    }
    private func makeBase(_ selectedHost: String) throws -> URL {
        let value = selectedHost.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let parts = URLComponents(string: "http://" + value), let host = parts.host, !host.isEmpty,
              parts.user == nil, parts.password == nil, parts.query == nil, parts.fragment == nil,
              parts.path.isEmpty, parts.port == nil || parts.port == 80, let url = parts.url else {
            throw ClientError.message("请输入设备的局域网 IP 或 .local 名称，不含路径。")
        }
        return url
    }
    func wifiAddressError(_ address: String) -> String? {
        do { _ = try makeBase(address); return nil }
        catch { return error.localizedDescription }
    }
    func connectWiFi(at address: String) async {
        guard connectionMode == .wifi, !connected, !connecting, !switching, !updating else { return }
        if let error = wifiAddressError(address) { note = error; return }
        // An explicit address starts a new target. Editing or cancelling the
        // sheet must not change the selected device or its recovery address.
        disconnect(note: "正在连接…")
        host = address.trimmingCharacters(in: .whitespacesAndNewlines)
        target = ConnectionTarget(host: host, deviceID: nil)
        await connect()
    }
    private func request(_ path: String, body: [String: Any]? = nil, authenticated: Bool = true) throws -> URLRequest {
        guard let base else { throw ClientError.message("请先选择设备") }
        var request = URLRequest(url: base.appendingPathComponent(path))
        if authenticated { request.setValue(key, forHTTPHeaderField: "X-ThinCube-Key") }
        if let body { request.httpMethod = "POST"; request.setValue("application/json", forHTTPHeaderField: "Content-Type"); request.httpBody = try JSONSerialization.data(withJSONObject: body) }
        return request
    }
    private func json(_ path: String, body: [String: Any]? = nil, authenticated: Bool = true) async throws -> [String: Any] {
        if activeBLE {
            var value = body ?? [:]
            switch path { case "api/info": value["op"] = "info"; case "api/pair": value["op"] = "pair"; case "api/status": value["op"] = "status"; case "api/settings": value["op"] = "settings"; default: throw ClientError.message("此操作需要 Wi-Fi，请先切换连接方式。") }
            return try await discovery.ble.exchange(value)
        }
        let (data, response) = try await session.data(for: request(path, body: body, authenticated: authenticated))
        let value = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] ?? [:]
        guard let http = response as? HTTPURLResponse, (200..<300).contains(http.statusCode) else {
            throw ClientError.http((response as? HTTPURLResponse)?.statusCode ?? 0,
                                   value["error"] as? String ?? value["err"] as? String ?? "设备请求失败")
        }
        return value
    }
    func selectDevice(_ id: String) {
        guard !connected, !connecting, !switching, !updating else { return }
        target = ConnectionTarget(host: "", deviceID: id.isEmpty ? nil : id)
        host = ""; pin = ""; transportPhase = ""
        refreshRecoveryAddress()
    }
    func selectConnectionMode(_ mode: ConnectionMode) async {
        guard canSelectConnectionMode, mode != connectionMode else { return }
        if connected, mode != .automatic, mode.rawValue != currentTransport {
            guard canSwitchTransport else {
                note = status?.bootVerified == false ? "设备启动检查中，请稍候再切换连接方式。" : "当前设备暂不能切换连接方式。"
                return
            }
            modeBeforeSwitch = connectionMode
            setConnectionMode(mode)
            await switchTransport(to: mode.rawValue)
            if !switching { restoreConnectionMode() }
            return
        }
        setConnectionMode(mode)
        transportPhase = ""
        if mode != .automatic { preferredTransport = mode.rawValue }
        if !connected {
            refreshRecoveryAddress()
            if mode != .wifi || pathSatisfied { recovery.resume(.network) }
            else { recovery.suspend(.network) }
            scheduleRecovery(immediate: true)
        }
    }
    private func setConnectionMode(_ mode: ConnectionMode) {
        connectionMode = mode; UserDefaults.standard.set(mode.rawValue, forKey: "connectionMode")
    }
    private func restoreConnectionMode() {
        if let modeBeforeSwitch { setConnectionMode(modeBeforeSwitch) }
        modeBeforeSwitch = nil
    }
    func connect() async {
        guard !updating, !connecting, !connected else { return }
        let selected = host.trimmingCharacters(in: .whitespacesAndNewlines)
        if selected != target.host { target = ConnectionTarget(host: selected, deviceID: nil) }
        if target.deviceID == nil, let found = discovery.devices.first(where: { $0.host == selected }) { target.deviceID = found.id }
        preferredTransport = connectionMode == .automatic ? (selected.hasPrefix("ble://") ? "ble" : "wifi") : connectionMode.rawValue
        refreshRecoveryAddress()
        guard !target.host.isEmpty, connectionMode.accepts(host: target.host) else {
            note = "尚未发现此设备的" + connectionMode.label + "连接。请在设备上选择对应模式，或使用自动模式。"; return
        }
        if connectionMode != .wifi || pathSatisfied { recovery.resume(.network) }
        recovery.request(); UserDefaults.standard.set(true, forKey: "autoReconnect")
        guard recovery.canAttempt else { note = "等待网络或 Mac 会话恢复后自动连接。"; return }
        await beginConnection(automatic: false)?.value
    }
    @discardableResult private func beginConnection(automatic: Bool) -> Task<Void, Never>? {
        guard recovery.canAttempt, !connecting, !connected, !updating else { return nil }
        refreshRecoveryAddress()
        guard !target.host.isEmpty, transportIntent != nil || connectionMode.accepts(host: target.host) else { return nil }
        teardown(note: automatic ? "正在自动恢复连接…" : "正在连接…")
        host = target.host; connecting = true; stopLocally = true
        let attempt = generation, selected = target
        let task = Task<Void, Never> { [weak self] in
            guard let self else { return }
            await self.performConnection(automatic: automatic, target: selected, attempt: attempt)
        }
        connectionTask = task
        return task
    }
    private func performConnection(automatic: Bool, target selected: ConnectionTarget, attempt: UUID) async {
        defer {
            if generation == attempt {
                connectionTask = nil
                if !connected { connecting = false }
            }
        }
        do {
            activeBLE = selected.host.hasPrefix("ble://")
            if activeBLE { base = nil; note = "正在连接蓝牙…"; try await discovery.ble.open(selected.host) }
            else { base = try makeBase(selected.host) }
            let info = try await json("api/info", authenticated: false)
            guard generation == attempt, !Task.isCancelled else { return }
            guard info["protocol"] as? Int == 1, let found = info["name"] as? String, found.hasPrefix("ThinCube-") else { throw ClientError.message("设备协议不兼容，请先安装 ThinCube v0.3 固件。") }
            guard selected.accepts(found) else { throw ClientError.message("此地址上的设备与所选设备不同，请重新选择设备并点击连接。") }
            name = found; version = info["version"] as? String ?? ""
            target.deviceID = found
            loadShortcut()
            var secret: String
            do {
                if info["paired"] as? Bool == false { throw CredentialError.missing }
                if let cachedCredential, cachedCredential.name == found { secret = cachedCredential.key }
                else {
                    note = "正在读取已保存授权；如出现系统钥匙串提示，请完成确认。"
                    secret = try await Task.detached { try Credentials.load(found) }.value
                }
            } catch CredentialError.missing {
                secret = try await authorizeDevice(found: found, attempt: attempt)
            }
            guard generation == attempt, !Task.isCancelled else { return }
            // Every BLE connection uses a fresh nonce; the key is never sent as the authentication proof.
            do { try await authenticate(secret: secret, found: found) }
            catch ClientError.http(let code, _) where code == 401 || code == 403 {
                cachedCredential = nil
                secret = try await authorizeDevice(found: found, attempt: attempt)
                try await authenticate(secret: secret, found: found)
            }
            guard generation == attempt, !Task.isCancelled else { return }
            cachedCredential = (found, secret)
            do { try prepareAudioNow() } catch { note = error.localizedDescription }
            if activeBLE {
                stopLocally = true; epoch = 0; ack = 0; keyStream = DeviceKeyStream(); lastStatus = Date(); lastHeartbeat = .distantPast
                // A status request follows the queued auth command, giving the main loop time to publish its new epoch.
                var validStatus = false
                for _ in 0..<10 {
                    let snapshot = try await discovery.ble.exchange(["op": "status"])
                    let data = try JSONSerialization.data(withJSONObject: snapshot)
                    if receive(data), epoch != 0 { validStatus = true; break }
                    try await Task.sleep(nanoseconds: 100_000_000)
                }
                guard validStatus else { throw ClientError.transient("BLE 未返回有效设备状态") }
                didConnect(found: found, address: selected.host, automatic: automatic)
                startTimer(); return
            }
            var components = URLComponents(url: base!.appendingPathComponent("control"), resolvingAgainstBaseURL: false)!
            components.scheme = "ws"
            var request = URLRequest(url: components.url!); request.setValue(key, forHTTPHeaderField: "X-ThinCube-Key")
            let socket = session.webSocketTask(with: request); websocket = socket; socket.resume()
            // Wait for a decoded control frame before reporting success. Gate
            // old streaming/held snapshots until the new peer is in standby.
            stopLocally = true; epoch = 0; ack = 0; keyStream = DeviceKeyStream(); lastStatus = Date(); lastHeartbeat = .distantPast
            let id = generation
            reader = Task { [weak self] in
                while !Task.isCancelled {
                    do {
                        let message = try await socket.receive()
                        let data: Data
                        switch message { case .data(let bytes): data = bytes; case .string(let text): data = Data(text.utf8); @unknown default: continue }
                        guard let self, self.generation == id else { return }
                        if self.receive(data), !self.connected {
                            self.didConnect(found: found, address: selected.host, automatic: automatic)
                        }
                    } catch {
                        guard let self, self.generation == id else { return }
                        self.connectionLost(note: "连接中断，语音与快捷键已释放：" + error.localizedDescription)
                        return
                    }
                }
            }
            startTimer()
            note = "正在确认控制连接…"
        } catch {
            guard generation == attempt else { return }
            connectionFailed(error)
        }
    }
    private func authenticate(secret: String, found: String) async throws {
        key = secret
        if activeBLE {
            let challenge = try await discovery.ble.exchange(["op": "challenge"])
            guard let nonce = challenge["nonce"] as? String, let binaryKey = Data(hex: secret) else { throw ClientError.message("BLE 认证挑战无效") }
            let message = Data("ThinCube-BLE-v1|\(found)|\(nonce)".utf8)
            let proof = HMAC<SHA256>.authenticationCode(for: message, using: SymmetricKey(data: binaryKey)).map { String(format: "%02x", $0) }.joined()
            _ = try await discovery.ble.exchange(["op": "auth", "proof": proof])
        }
        _ = try await json("api/status")
    }
    private func authorizeDevice(found: String, attempt: UUID) async throws -> String {
        var hint = "请输入设备屏幕上的六位配对码；配对后 Wi-Fi 和蓝牙共用已保存的授权。"
        while generation == attempt, !Task.isCancelled {
            pin = ""; pairingRequired = true; note = hint
            let code = try await withCheckedThrowingContinuation { pairingContinuation = $0 }
            guard generation == attempt, !Task.isCancelled else { throw CancellationError() }
            note = "正在验证配对码…"
            do {
                let result = try await json("api/pair", body: ["pin": code], authenticated: false)
                guard generation == attempt, !Task.isCancelled else { throw CancellationError() }
                guard let pairedKey = result["key"] as? String, pairedKey.count == 32 else { throw ClientError.message("授权响应无效") }
                try await Task.detached { try Credentials.save(pairedKey, name: found) }.value
                guard generation == attempt, !Task.isCancelled else { throw CancellationError() }
                return pairedKey
            } catch ClientError.http(let status, let error) where (status == 400 || status == 403) && error == "invalid_pin_or_window" {
                hint = "配对码有误或已过期。请核对 PAIR PIN；如已过期，同时按住 A+B 3 秒并松开，重新连接后输入新码。"
            }
        }
        throw CancellationError()
    }
    func submitPairingCode() {
        guard canSubmitPairingCode, let continuation = pairingContinuation else { return }
        pairingContinuation = nil; pairingRequired = false
        let code = pin; pin = ""; continuation.resume(returning: code)
    }
    private func startTimer() {
        timer?.invalidate(); timer = Timer(timeInterval: 0.05, repeats: true) { [weak self] _ in Task { @MainActor in self?.tick() } }
        RunLoop.main.add(timer!, forMode: .common)
    }
    private func didConnect(found: String, address: String, automatic: Bool) {
        connected = true; connecting = false; recovery.didConnect()
        target = ConnectionTarget(host: address, deviceID: found); host = address
        preferredTransport = activeBLE ? "ble" : "wifi"
        UserDefaults.standard.set(address, forKey: "lastHost")
        UserDefaults.standard.set(address, forKey: (activeBLE ? "bleHost." : "wifiHost.") + found)
        UserDefaults.standard.set(found, forKey: "lastDeviceName"); UserDefaults.standard.set(true, forKey: "autoReconnect")
        note = audioPrepared ? "已连接，语音待命。" : "已连接；请安装 BlackHole 2ch 后准备音频。"
        if activeBLE && status?.transportAudioReady == false { note = "BLE 控制已连接，但当前链路 MTU 不足以传输语音。请尝试重新连接或切回 Wi-Fi。" }
        keyDiagnostics.record("连接", "\(automatic ? "自动恢复" : "连接")成功：\(found) · \(transportLabel)")
        // A physical B press can change the mode again while the host is
        // offline. A fresh authenticated connection tells us the actual choice.
        if switching, let intent = transportIntent, let transaction = status?.transportTransaction,
           transaction.phase == "committed", transaction.requestId != intent.id {
            switching = false; transportIntent = nil; modeBeforeSwitch = nil
            if connectionMode != .automatic { setConnectionMode(currentTransport == "ble" ? .ble : .wifi) }
            transportPhase = "已连接设备当前模式：" + transportLabel; note = transportPhase
        }
        if let status { handleTransport(status) }
    }
    // Opt-in hardware validation: open an encrypted BLE link without proving
    // the saved application key. No credential is read or exposed by this test.
    func checkUnauthenticatedBLEAccess() async throws {
        guard connected, activeBLE, !switching, !updating, status?.streaming == false else {
            throw ClientError.message("请在蓝牙已连接且未传音时验证授权拦截。")
        }
        let address = host, identity = name
        recovery.disconnect(); teardown(note: "正在验证蓝牙授权拦截…")
        defer { discovery.ble.close(); recovery.request(); scheduleRecovery(immediate: true) }
        try await discovery.ble.open(address)
        let info = try await discovery.ble.exchange(["op": "info"])
        guard info["name"] as? String == identity else { throw ClientError.message("授权测试设备身份不匹配") }
        for request: [String: Any] in [["op": "status"], ["op": "settings"], ["op": "start"],
                                       ["op": "transport_switch"], ["op": "auth", "proof": String(repeating: "0", count: 64)],
                                       ["op": "status"]] {
            do {
                _ = try await discovery.ble.exchange(request)
                throw ClientError.message("未授权的蓝牙请求被错误接受")
            } catch ClientError.http(let code, let error) where code == 401 && error == "pairing_required" {
                // Expected: transport encryption alone never grants control.
            }
        }
    }
    private func sendControl(_ payload: [String: Any]) async throws {
        if activeBLE { try await discovery.ble.send(payload) }
        else if let websocket {
            let data = try JSONSerialization.data(withJSONObject: payload); try await websocket.send(.string(String(decoding: data, as: UTF8.self)))
        } else { throw ClientError.message("控制连接未就绪") }
    }
    func switchTransport(to requested: String? = nil) async {
        guard canSwitchTransport else { return }
        let desired = requested ?? (currentTransport == "ble" ? "wifi" : "ble")
        guard ["wifi", "ble"].contains(desired), desired != currentTransport else { return }
        if desired == "ble", let hint = discovery.ble.permissionHint { note = hint; return }
        guard cachedCredential != nil else { note = "请先完成配对再切换连接方式。"; return }
        let id = UUID().uuidString
        transportIntent = (id, desired, currentTransport, Date()); preferredTransport = desired; switching = true; transportPhase = "正在请求切换…"
        stopLocally = true; keys.release(); buffer.setSession(nil); bleAudio.reset()
        do { try await sendControl(["op": "transport_switch", "epoch": epoch, "target": desired, "requestId": id]) }
        catch { connectionLost(note: "切换请求确认暂未收到，正在查找设备状态。") }
    }
    private func handleTransport(_ snapshot: DeviceStatus) {
        guard connected else { return }
        if let intent = transportIntent, snapshot.rejectedRequest == intent.id, let error = snapshot.transportError, !error.isEmpty {
            let reason: String
            switch error {
            case "wifi_unconfigured": reason = "尚未配置 Wi-Fi，请开机按住 B 进入配网。"
            case "boot_unverified": reason = "启动健康检查尚未通过，请等待后重试。"
            case "busy": reason = "设备正在处理其他操作，请稍后重试。"
            case "storage_failed": reason = "设备无法保存连接模式，请检查后重试。"
            default: reason = "设备未接受本次切换，请重新连接后重试。"
            }
            switching = false; transportIntent = nil; restoreConnectionMode(); preferredTransport = currentTransport
            transportPhase = "切换未执行：" + reason; note = transportPhase; return
        }
        guard let transaction = snapshot.transportTransaction, !transaction.requestId.isEmpty else { return }
        if transaction.phase == "pending" || transaction.phase == "trial" {
            if transportIntent?.id != transaction.requestId { transportIntent = (transaction.requestId, transaction.target, transaction.previous, Date()) }
            switching = true; keys.release(); buffer.setSession(nil); bleAudio.reset()
            if transaction.phase == "pending" {
                transportPhase = "设备正在重启，切换至 " + (transaction.target == "ble" ? "蓝牙" : "Wi-Fi")
                preferredTransport = transaction.target
                let id = generation
                Task { [weak self] in
                    try? await Task.sleep(nanoseconds: 150_000_000)
                    guard let self, self.generation == id, self.switching else { return }
                    self.connectionLost(note: self.transportPhase)
                }
            } else if !commitInFlight {
                transportPhase = "已认证目标连接，正在确认切换…"; commitInFlight = true
                let id = generation
                Task { [weak self] in
                    guard let self else { return }
                    defer { if self.generation == id { self.commitInFlight = false } }
                    do { try await self.sendControl(["op": "transport_commit", "epoch": self.epoch, "requestId": transaction.requestId]) }
                    catch { if self.generation == id { self.connectionLost(note: "确认切换时连接中断，正在恢复。") } }
                }
            }
        } else if switching, let intent = transportIntent, transaction.requestId == intent.id {
            switching = false; transportIntent = nil
            if transaction.phase == "rolled_back" { restoreConnectionMode() }
            else { modeBeforeSwitch = nil }
            if connectionMode != .automatic { setConnectionMode(currentTransport == "ble" ? .ble : .wifi) }
            transportPhase = transaction.phase == "rolled_back" ? "目标连接未确认，设备已恢复原连接模式。" : "切换完成：" + transportLabel
            note = transportPhase
        }
    }
    @discardableResult private func receive(_ data: Data) -> Bool {
        let next: DeviceStatus
        do { next = try JSONDecoder().decode(DeviceStatus.self, from: data) }
        catch {
            if Date().timeIntervalSince(lastKeyDecodeFailure) > 2 {
                keyDiagnostics.record("协议", "无法解析设备状态：\(error.localizedDescription)")
                lastKeyDecodeFailure = Date()
            }
            return false
        }
        if let id = next.deviceId, id != name { return false }
        lastStatus = Date()
        let blocked = shortcutBlock(next)
        let keyUpdate = keyStream.receive(session: next.epoch, edge: next.edge, canInject: blocked == nil)
        if keyUpdate.sessionChanged {
            keys.release(); buffer.setSession(nil); epoch = next.epoch
            keyDiagnostics.record("连接", "按键会话 epoch=\(next.epoch)，已释放上一会话按键。")
        }
        // Preserve unsaved slider edits while device snapshots arrive ten times a second.
        if status == nil || gain == status?.gain { gain = next.gain }
        if status == nil || brightness == Double(status!.brightness) { brightness = Double(next.brightness) }
        if status == nil || soundVolume == Double(status?.soundVolume ?? 35) { soundVolume = Double(next.soundVolume ?? 35) }
        status = next
        handleTransport(next)
        if let pending = pendingCalibration, let report = next.calibration,
           report.op == pending.op, report.seq != pending.seq {
            let label = pending.op == "calibrate_faceup" ? "屏幕朝上平放" : "竖向倒立"
            note = report.ok ? label + "校准成功并已保存。屏幕朝上平放后再倒立即可传音。" : label + "校准未通过，请按提示摆正并保持不动后重试；原校准保留。"
            pendingCalibration = nil; calibrationBusy = false
        }
        if !next.streaming || next.muted || !next.allowed || updating || switching || stopLocally {
            buffer.setSession(nil)
        } else { buffer.setSession(next.session) }
        if let edge = keyUpdate.edge {
            let action = edge.down ? (blocked.map { "拦截：" + $0 } ?? "准备注入 \(shortcut.label)") : "释放快捷键"
            keyDiagnostics.record("设备 A", "\(edge.down ? "按下" : "松开") · seq=\(edge.seq) · held=\(next.held ? 1 : 0) · inverted=\(next.inverted ? 1 : 0) · streaming=\(next.streaming ? 1 : 0) · \(action)")
        }
        if keyUpdate.release { keys.release() }
        if keyUpdate.press { _ = keys.press(shortcut) }
        ack = keyStream.acknowledgement
        if stopLocally && !next.streaming { stopLocally = false }
        return true
    }
    private func shortcutBlock(_ status: DeviceStatus) -> String? {
        if !connected || connecting { return "控制连接尚未确认" }
        if updating { return "固件升级中" }
        if switching { return "连接模式切换中" }
        if stopLocally { return "已在 Mac 停止语音" }
        if !status.allowed { return "语音输入未开启" }
        if status.muted { return "麦克风已静音" }
        if !status.streaming { return "设备未处于传音状态" }
        if !status.inverted { return "设备未处于倒置语音姿态" }
        if shortcutRecording { return "正在编辑快捷键" }
        if shortcutTestRunning { return "本地注入测试进行中" }
        return nil
    }

    func testCurrentShortcut() {
        guard !shortcutTestRunning, !shortcutRecording, !updating, !connecting else { return }
        keys.release()
        keyDiagnostics.start()
        keyDiagnostics.refreshPermission()
        guard keyDiagnostics.monitoring else {
            keyDiagnostics.record("测试", "请激活 ThinCube 窗口后再测试。")
            return
        }
        keyDiagnostics.record("测试", "测试当前快捷键 \(shortcut.label)；150 ms 后自动松开。")
        guard testKeys.press(shortcut) else { return }
        shortcutTestRunning = true
        shortcutTestTask = Task { [weak self] in
            do { try await Task.sleep(nanoseconds: 150_000_000) } catch { return }
            guard let self, !Task.isCancelled else { return }
            self.testKeys.release()
            self.shortcutTestRunning = false; self.shortcutTestTask = nil
        }
    }

    private func cancelShortcutTest() {
        shortcutTestTask?.cancel(); shortcutTestTask = nil
        testKeys.release(); shortcutTestRunning = false
    }

    func stopKeyboardTest() { keyDiagnostics.stop(); cancelShortcutTest() }

    private func tick() {
        let now = Date()
        if let pending = pendingCalibration, now > pending.deadline {
            pendingCalibration = nil; calibrationBusy = false
            note = "本次校准没有收到设备确认，请检查连接后重试。"
        }
        if let intent = transportIntent, switching, connected,
           status?.transportTransaction?.requestId != intent.id,
           now.timeIntervalSince(intent.started) > 4 {
            if now.timeIntervalSince(intent.started) > 15 {
                switching = false; transportIntent = nil; preferredTransport = currentTransport; restoreConnectionMode()
                transportPhase = "设备未确认切换，请重试。"; note = transportPhase
            } else if now.timeIntervalSince(lastSwitchRetry) > 1 {
                lastSwitchRetry = now
                Task { [weak self] in try? await self?.sendControl(["op": "transport_switch", "epoch": self?.epoch ?? 0, "target": intent.target, "requestId": intent.id]) }
            }
        }
        if now.timeIntervalSince(lastDiagnostics) >= 1 {
            let stats = buffer.statistics()
            audioDiagnostics = "接收 \(stats.receivedFrames) 帧 · 写入音频桥 \(stats.renderedSamples) 样本 · 丢失补零 \(stats.concealedFrames) 帧 · 缓冲等待 \(String(format: "%.0f", Double(stats.waitingSamples)/48)) ms · 峰值 \(String(format: "%.1f", 20 * log10(max(stats.peak, 0.00001)))) dBFS"
            lastDiagnostics = now
        }
        if now.timeIntervalSince(lastStatus) > 0.4 {
            if keys.isActive { keyDiagnostics.record("保护", "设备状态超过 400 ms 未更新，释放快捷键。") }
            keys.release(); buffer.setSession(nil)
        }
        if now.timeIntervalSince(lastStatus) > 2 { connectionLost(note: "设备未响应，语音与快捷键已释放。"); return }
        guard !sendingHeartbeat, now.timeIntervalSince(lastHeartbeat) >= (activeBLE ? 0.2 : 0.1), activeBLE || websocket != nil else { return }
        sendingHeartbeat = true; lastHeartbeat = now
        let id = generation
        let payload: [String: Any] = ["op": "heartbeat", "epoch": epoch, "ack": ack, "port": Int(receiver.port), "ready": connected && audioPrepared && !updating && !switching]
        Task { [weak self] in
            defer { if self?.generation == id { self?.sendingHeartbeat = false } }
            guard let self, self.generation == id else { return }
            do { try await self.sendControl(payload) }
            catch { if self.generation == id { self.connectionLost(note: "控制连接中断，已释放快捷键。") } }
        }
    }
    func command(_ op: String) async {
        if op == "stop" { stopLocally = true; keys.release(); buffer.setSession(nil) }
        guard connected, !switching else { return }
        do {
            // An immediate click after connection must establish the audio lease
            // before start, even if the first periodic heartbeat has not run yet.
            if op == "start" {
                guard audioPrepared, !updating else { return }
                try await sendControl(["op": "heartbeat", "epoch": epoch, "ack": ack, "port": Int(receiver.port), "ready": true])
            }
            try await sendControl(["op": op, "epoch": epoch])
        } catch { note = error.localizedDescription }
    }
    func calibrate(_ op: String) async {
        guard connected, !updating, !calibrationBusy else { return }
        calibrationBusy = true
        pendingCalibration = (op, status?.calibration?.seq ?? 0, Date().addingTimeInterval(4))
        note = "正在校准，请保持设备不动…"
        await command("stop")
        await command(op)
    }
    func scheduleSoundVolume(_ value: Int) {
        soundUpdateTask?.cancel()
        guard connected, value != status?.soundVolume else { return }
        soundUpdateTask = Task { [weak self] in
            do { try await Task.sleep(nanoseconds: 300_000_000) } catch { return }
            guard let self, self.connected, !self.updating else { return }
            await self.apply(["soundVolume": value])
        }
    }
    func apply(_ settings: [String: Any]) async {
        if settings["muted"] as? Bool == true || settings["allowed"] as? Bool == false { stopLocally = true; keys.release(); buffer.setSession(nil) }
        do { _ = try await json("api/settings", body: settings) } catch { note = error.localizedDescription }
    }
    private func prepareAudioNow() throws {
        try bridge.start()
        do { if !activeBLE { try receiver.start(buffer: buffer) }; audioPrepared = true }
        catch { bridge.stop(); audioPrepared = false; throw error }
    }
    func prepareAudio() { do { try prepareAudioNow(); note = "BlackHole 2ch 已准备，采样率 48 kHz。" } catch { audioPrepared = false; note = error.localizedDescription } }
    func selectSystemInput() { do { try bridge.selectSystemInput(); note = "系统输入已设为 BlackHole 2ch。也可在目标应用中单独选择它。" } catch { note = error.localizedDescription } }
    func setShortcutRecording(_ recording: Bool) {
        if recording { stopKeyboardTest() }
        keys.release()
        shortcutRecordingCount = max(0, shortcutRecordingCount + (recording ? 1 : -1))
        shortcutRecording = shortcutRecordingCount > 0
    }
    @discardableResult func setShortcut(_ value: Shortcut) -> Bool {
        guard value.isValid, !connecting, !updating else { return false }
        stopKeyboardTest()
        do {
            let data = try JSONEncoder().encode(value)
            keys.release()
            UserDefaults.standard.set(data, forKey: "shortcut." + name)
            shortcut = value
            return true
        } catch {
            note = "快捷键保存失败：" + error.localizedDescription
            return false
        }
    }
    private func loadShortcut() {
        keys.release()
        let data = UserDefaults.standard.data(forKey: "shortcut." + name)
            ?? UserDefaults.standard.data(forKey: "shortcut.ThinCube")
        if let data, let value = try? JSONDecoder().decode(Shortcut.self, from: data) { shortcut = value }
        else { shortcut = Shortcut() }
    }
    @discardableResult func changeWifi() async -> Bool {
        defer { wifiPassword = "" }
        guard connected, !updating, !wifiSSID.isEmpty, !activeBLE, !switching else { note = "请先连接 Wi-Fi，再修改网络。"; return false }
        await command("stop")
        do {
            _ = try await json("api/wifi", body: ["ssid": wifiSSID, "password": wifiPassword])
            wifiPassword = ""; connectionLost(note: "设备正在验证新的 Wi-Fi，成功后重启。Mac 接入同一网络后将自动恢复连接；失败可通过设备热点重试。")
            return true
        } catch { note = error.localizedDescription; return false }
    }
    func chooseFirmware() {
        guard !activeBLE, !switching else { note = "固件升级需要 Wi-Fi，请先切换连接模式。"; return }
        let panel = NSOpenPanel(); panel.allowedContentTypes = [.data]; panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        Task { await update(url) }
    }
    private func update(_ url: URL) async {
        guard connected, !updating else { return }
        do {
            let image = try FirmwareImage(data: Data(contentsOf: url))
            await command("stop"); updating = true; recovery.suspend(.update)
            reconnectTask?.cancel(); reconnectTask = nil; reconnecting = false
            keys.release(); buffer.setSession(nil); updateProgress = 0
            defer { updating = false; recovery.resume(.update); scheduleRecovery() }
            let begin = try await json("ota/begin", body: ["ver": image.version, "size": image.data.count, "sha256": image.sha256])
            guard let token = begin["token"] as? String else { throw ClientError.message("OTA 会话创建失败") }
            note = "请在设备上按 A 确认升级，按 B 取消。"
            let deadline = Date().addingTimeInterval(35)
            var ready = false
            while Date() < deadline {
                let status = try await json("ota/status")
                if status["stage"] as? String == "ready" { ready = true; break }
                if status["stage"] as? String == "failed" { throw ClientError.message(status["err"] as? String ?? "升级已取消") }
                try await Task.sleep(nanoseconds: 500_000_000)
            }
            guard ready else { throw ClientError.message("设备确认超时") }
            note = "正在通过 Wi-Fi 写入固件…"
            var request = try self.request("ota"); request.httpMethod = "POST"; request.timeoutInterval = 130
            request.setValue(token, forHTTPHeaderField: "X-ThinCube-OTA"); request.setValue("application/octet-stream", forHTTPHeaderField: "Content-Type")
            let progress = UploadProgress(); progress.changed = { [weak self] value in Task { @MainActor in self?.updateProgress = value } }
            let (data, response) = try await session.upload(for: request, from: image.data, delegate: progress)
            guard (response as? HTTPURLResponse)?.statusCode == 200,
                  (try? JSONSerialization.jsonObject(with: data) as? [String: Bool])?["ok"] == true else { throw ClientError.message("固件写入或校验失败；原固件仍保留。") }
            updateProgress = 1
            connectionLost(note: "固件写入并校验成功，设备重启后将自动恢复连接；新固件稳定运行 60 秒后才通过启动健康确认。")
        } catch { keys.release(); buffer.setSession(nil); note = error.localizedDescription }
    }
    func disconnect(note message: String = "已断开") {
        recovery.disconnect(); switching = false; transportIntent = nil; modeBeforeSwitch = nil; transportPhase = ""; UserDefaults.standard.set(false, forKey: "autoReconnect")
        teardown(note: message)
    }
    func shutdown() {
        recovery.suspend(.termination); pathMonitor.cancel()
        teardown(note: "")
    }
    private func connectionLost(note message: String) {
        teardown(note: message)
        scheduleRecovery()
    }
    private func connectionFailed(_ error: Error) {
        let permanent: Bool
        switch error {
        case is CredentialError: permanent = true
        case ClientError.message: permanent = true
        case ClientError.http(let code, _): permanent = [400, 401, 403, 404].contains(code)
        default: permanent = false
        }
        if permanent && !switching { recovery.requireUserAction(); cachedCredential = nil }
        if !switching && !permanent && connectionMode == .automatic { preferredTransport = preferredTransport == "ble" ? "wifi" : "ble" }
        connectionLost(note: error.localizedDescription + (permanent ? " 请处理后点击“连接”重试。" : ""))
    }
    private func suspendRecovery(_ reason: ConnectionRecovery.Suspension, note message: String) {
        recovery.suspend(reason)
        teardown(note: recovery.enabled ? message : "")
    }
    private func resumeRecovery(_ reason: ConnectionRecovery.Suspension) {
        recovery.resume(reason)
        scheduleRecovery(immediate: true)
    }
    private func refreshRecoveryAddress() {
        let resolvingMode: ConnectionMode
        if let intent = transportIntent {
            preferredTransport = intent.target
            if Date().timeIntervalSince(intent.started) > 15 {
                transportPhase = "已选择" + (intent.target == "ble" ? "蓝牙" : "Wi-Fi") + "，等待设备连接；可长按设备 B 键再次切换。"
            }
        }
        if transportIntent != nil && connectionMode != .automatic { resolvingMode = ConnectionMode(rawValue: preferredTransport) ?? .automatic }
        else { resolvingMode = connectionMode }
        guard let id = target.deviceID else { return }
        let endpoints = discovery.devices.map { ConnectionEndpoint(deviceID: $0.id, host: $0.host) }
        if let match = resolvingMode.endpoint(for: id, in: endpoints, preferredTransport: preferredTransport,
                                             wifiAvailable: pathSatisfied, bluetoothAvailable: discovery.ble.permissionHint == nil) {
            target.host = match.host; host = match.host
            preferredTransport = match.host.hasPrefix("ble://") ? "ble" : "wifi"
        } else {
            if resolvingMode != .automatic { preferredTransport = resolvingMode.rawValue }
            else if !pathSatisfied { preferredTransport = "ble" }
            else if discovery.ble.permissionHint != nil { preferredTransport = "wifi" }
            if let cached = UserDefaults.standard.string(forKey: (preferredTransport == "ble" ? "bleHost." : "wifiHost.") + id) {
                target.host = cached; host = cached
            } else if !resolvingMode.accepts(host: target.host) {
                target.host = ""; host = ""
            }
        }
    }
    private func discovered(_ devices: [FoundDevice]) {
        guard !connected, !connecting, !updating else { return }
        // The publisher delivers before Discovery.devices changes. Resolve with
        // the delivered snapshot so switching devices can wake recovery immediately.
        guard let id = target.deviceID else { return }
        let mode = transportIntent == nil || connectionMode == .automatic ? connectionMode : (ConnectionMode(rawValue: preferredTransport) ?? .automatic)
        guard let match = mode.endpoint(for: id, in: devices.map { ConnectionEndpoint(deviceID: $0.id, host: $0.host) },
                                       preferredTransport: preferredTransport, wifiAvailable: pathSatisfied,
                                       bluetoothAvailable: discovery.ble.permissionHint == nil), target.host != match.host else { return }
        target.host = match.host; host = match.host; preferredTransport = match.host.hasPrefix("ble://") ? "ble" : "wifi"
        if connectionMode != .wifi || preferredTransport == "ble" { recovery.resume(.network) }
        if recovery.enabled { scheduleRecovery(immediate: true) }
    }
    private func scheduleRecovery(immediate: Bool = false) {
        guard recovery.canAttempt, !connected, !connecting, !updating, !target.host.isEmpty else { return }
        if reconnectTask != nil && !immediate { return }
        reconnectTask?.cancel()
        refreshRecoveryAddress()
        if preferredTransport == "ble" { recovery.resume(.network) }
        discovery.refresh(id: target.deviceID)
        let delay = immediate ? 0 : (recovery.nextDelay() ?? 30)
        reconnecting = true
        if delay > 0 { note += " \(Int(delay)) 秒后自动重试。" }
        let id = generation
        reconnectTask = Task { [weak self] in
            do { try await Task.sleep(nanoseconds: UInt64(delay * 1_000_000_000)) } catch { return }
            guard let self, !Task.isCancelled, self.generation == id, self.recovery.canAttempt else { return }
            self.reconnectTask = nil; self.reconnecting = false
            self.beginConnection(automatic: true)
        }
    }
    private func teardown(note message: String) {
        pairingRequired = false; pin = ""
        let authorization = pairingContinuation; pairingContinuation = nil
        authorization?.resume(throwing: CancellationError())
        stopKeyboardTest()
        if connected { keyDiagnostics.record("连接", "断开：" + message + "；已释放快捷键。") }
        soundUpdateTask?.cancel(); soundUpdateTask = nil
        pendingCalibration = nil; calibrationBusy = false
        generation = UUID(); reconnectTask?.cancel(); reconnectTask = nil; reconnecting = false
        connectionTask?.cancel(); connectionTask = nil; connecting = false
        timer?.invalidate(); timer = nil; reader?.cancel(); reader = nil
        websocket?.cancel(with: .goingAway, reason: nil); websocket = nil
        keys.release(); buffer.setSession(nil); bleAudio.reset(); receiver.stop(); bridge.stop(); connected = false; audioPrepared = false
        activeBLE = false; discovery.ble.close(); commitInFlight = false
        status = nil; sendingHeartbeat = false; stopLocally = false
        if !message.isEmpty { note = message }
    }
}
