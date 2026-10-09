import CoreBluetooth
import Foundation
import ThinCubeCore

@MainActor final class BLEConnection: NSObject, ObservableObject, @preconcurrency CBCentralManagerDelegate, @preconcurrency CBPeripheralDelegate {
    static let service = CBUUID(string: "8D7A1000-6D43-4F3B-9AC8-4C7E62A8B101")
    private let controlID = CBUUID(string: "8D7A1001-6D43-4F3B-9AC8-4C7E62A8B101")
    private let audioID = CBUUID(string: "8D7A1002-6D43-4F3B-9AC8-4C7E62A8B101")
    @Published private(set) var devices: [FoundDevice] = []
    var onStatus: ((Data) -> Void)?, onAudio: ((Data) -> Void)?, onDisconnect: ((Error) -> Void)?
    private var manager: CBCentralManager!
    private var peripherals: [UUID: CBPeripheral] = [:]
    private var lastSeen: [String: Date] = [:]
    private var discoveryTimer: Timer?
    private var expectedDisconnects = Set<UUID>()
    private var peripheral: CBPeripheral?, control: CBCharacteristic?, audio: CBCharacteristic?
    private var opening: CheckedContinuation<Void, Error>?, openTimeout: Task<Void, Never>?
    private var requests: [Int: CheckedContinuation<[String: Any], Error>] = [:]
    private var requestTimers: [Int: Task<Void, Never>] = [:]
    private struct Write { var parts: [Data]; var index = 0; let continuation: CheckedContinuation<Void, Error> }
    private var writes: [Write] = [], writing = false
    private var nextRequest = 0, nextMessage: UInt16 = 0
    private var controlFragments = BLEFragments(), audioFragments = BLEFragments(maximum: 348, timeout: 0.2)
    private var snapshots = BLEStatusSnapshots()
    private func trace(_ text: String) {
        if ProcessInfo.processInfo.arguments.contains("--hardware-check") { print("[BLE] \(text)") }
    }
    override init() {
        super.init(); manager = CBCentralManager(delegate: self, queue: .main)
        discoveryTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.expireDiscovery() }
        }
    }
    private func expireDiscovery() {
        let expired = devices.filter { Date().timeIntervalSince(lastSeen[$0.host] ?? .distantPast) > 8 }
        guard !expired.isEmpty else { return }
        for device in expired { lastSeen.removeValue(forKey: device.host) }
        devices.removeAll { expired.contains($0) }
    }
    var audioCapable: Bool { peripheral.map { $0.maximumWriteValueLength(for: .withResponse) >= 182 } ?? false }
    var permissionHint: String? {
        switch manager.state {
        case .poweredOn: return nil
        case .unauthorized: return "请在系统设置 → 隐私与安全性 → 蓝牙中允许 ThinCube。"
        case .poweredOff: return "请打开 Mac 蓝牙。"
        case .unsupported: return "此 Mac 无法使用 BLE。"
        default: return "正在准备 Mac 蓝牙…"
        }
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn { central.scanForPeripherals(withServices: [Self.service], options: [CBCentralManagerScanOptionAllowDuplicatesKey: true]) }
        else { fail(ClientError.message(permissionHint ?? "蓝牙连接不可用"), notify: peripheral != nil); devices = [] }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name, name.hasPrefix("ThinCube-") else { return }
        peripherals[peripheral.identifier] = peripheral
        let found = FoundDevice(id: name, host: "ble://" + peripheral.identifier.uuidString)
        lastSeen[found.host] = Date()
        if let index = devices.firstIndex(where: { $0.id == name }) {
            if devices[index] != found { devices[index] = found }
        } else { devices.append(found) }
    }
    func open(_ address: String) async throws {
        close()
        for _ in 0..<20 {
            if manager.state != .unknown && manager.state != .resetting { break }
            try await Task.sleep(nanoseconds: 100_000_000)
        }
        guard manager.state == .poweredOn else {
            if manager.state == .unknown || manager.state == .resetting { throw ClientError.transient("Mac 蓝牙正在准备，稍后自动重试。") }
            throw ClientError.message(permissionHint ?? "蓝牙不可用")
        }
        guard let uuid = UUID(uuidString: String(address.dropFirst(6))),
              let found = peripherals[uuid] ?? manager.retrievePeripherals(withIdentifiers: [uuid]).first else { throw ClientError.transient("尚未发现 BLE 设备，请稍候或长按设备 B 切到 BLE。") }
        peripheral = found; found.delegate = self
        try await withCheckedThrowingContinuation { continuation in
            opening = continuation; manager.connect(found, options: nil)
            openTimeout = Task { [weak self] in
                do { try await Task.sleep(nanoseconds: 12_000_000_000) } catch { return }
                self?.fail(ClientError.transient("设备蓝牙暂不可达，正在等待设备。"), notify: false)
            }
        }
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        trace("connected")
        guard peripheral == self.peripheral else { return }
        openTimeout?.cancel()
        openTimeout = Task { [weak self] in
            do { try await Task.sleep(nanoseconds: 120_000_000_000) } catch { return }
            self?.fail(ClientError.message("蓝牙配对超时。请在 macOS 配对弹窗中输入 BLUETOOTH PIN 码，再点击连接重试。"), notify: false)
        }
        peripheral.discoverServices([Self.service])
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard peripheral == self.peripheral else { return }; fail(error ?? ClientError.transient("BLE 连接失败"), notify: false)
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        if expectedDisconnects.remove(peripheral.identifier) != nil { return }
        guard peripheral == self.peripheral else { return }; fail(error ?? ClientError.message("BLE 已断开"), notify: true)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard peripheral == self.peripheral else { return }
        if let error { fail(error, notify: false); return }
        guard let service = peripheral.services?.first(where: { $0.uuid == Self.service }) else { fail(ClientError.message("BLE 服务不兼容"), notify: false); return }
        peripheral.discoverCharacteristics([controlID, audioID], for: service)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard peripheral == self.peripheral else { return }
        if let error { fail(error, notify: false); return }
        control = service.characteristics?.first { $0.uuid == controlID }; audio = service.characteristics?.first { $0.uuid == audioID }
        guard let control, let audio else { fail(ClientError.message("BLE 特征不完整"), notify: false); return }
        peripheral.setNotifyValue(true, for: control); peripheral.setNotifyValue(true, for: audio)
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral == self.peripheral else { return }
        if let error { fail(error, notify: false); return }
        trace("subscribe \(characteristic.uuid == controlID ? "control" : "audio") enabled=\(characteristic.isNotifying)")
        if control?.isNotifying == true && audio?.isNotifying == true {
            openTimeout?.cancel(); openTimeout = nil; opening?.resume(); opening = nil
        }
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral == self.peripheral, error == nil, let bytes = characteristic.value else { return }
        let now = ProcessInfo.processInfo.systemUptime
        if characteristic.uuid == audioID {
            if let data = audioFragments.accept(bytes, now: now) { onAudio?(data) }; return
        }
        if bytes.count >= 8 {
            let b = [UInt8](bytes.prefix(8))
            trace("control fragment bytes=\(bytes.count) id=\(Int(b[2])*256+Int(b[3])) offset=\(Int(b[4])*256+Int(b[5])) total=\(Int(b[6])*256+Int(b[7]))")
        }
        guard let frame = controlFragments.accept(bytes, now: now), let data = snapshots.accept(frame),
              let value = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        if let rid = value["rid"] as? Int, let continuation = requests.removeValue(forKey: rid) {
            requestTimers.removeValue(forKey: rid)?.cancel()
            if let error = value["error"] as? String { continuation.resume(throwing: ClientError.http(error == "pairing_required" ? 401 : 400, error)) }
            else { continuation.resume(returning: value) }
        }
        if value["epoch"] != nil { onStatus?(data) }
    }
    func exchange(_ value: [String: Any]) async throws -> [String: Any] {
        nextRequest += 1; let rid = nextRequest; var body = value; body["rid"] = rid
        trace("exchange op=\(value["op"] as? String ?? "") rid=\(rid)")
        return try await withCheckedThrowingContinuation { continuation in
            requests[rid] = continuation
            requestTimers[rid] = Task { [weak self] in
                do { try await Task.sleep(nanoseconds: 10_000_000_000) } catch { return }
                self?.requests.removeValue(forKey: rid)?.resume(throwing: ClientError.transient("BLE 请求超时")); self?.requestTimers.removeValue(forKey: rid)
            }
            Task { [weak self] in
                guard let self, self.requests[rid] != nil else { return }
                do { try await self.send(body) }
                catch { self.requests.removeValue(forKey: rid)?.resume(throwing: error); self.requestTimers.removeValue(forKey: rid)?.cancel() }
            }
        }
    }
    func send(_ value: [String: Any]) async throws {
        guard let peripheral, control != nil else { throw ClientError.message("BLE 尚未连接") }
        guard writes.count < 16 else { throw ClientError.message("BLE 控制队列繁忙") }
        let data = try JSONSerialization.data(withJSONObject: value); nextMessage &+= 1
        let parts = BLEFragments.split(data, id: nextMessage, valueLength: min(512, peripheral.maximumWriteValueLength(for: .withResponse)))
        guard !parts.isEmpty else { throw ClientError.message("BLE 消息过长") }
        try await withCheckedThrowingContinuation { continuation in writes.append(Write(parts: parts, continuation: continuation)); pump() }
    }
    private func pump() {
        guard !writing, let peripheral, let control, let job = writes.first else { return }
        writing = true; peripheral.writeValue(job.parts[job.index], for: control, type: .withResponse)
    }
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        guard peripheral == self.peripheral, characteristic.uuid == controlID, !writes.isEmpty else { return }
        writing = false
        if let error { let job = writes.removeFirst(); job.continuation.resume(throwing: error) }
        else { writes[0].index += 1; if writes[0].index == writes[0].parts.count { writes.removeFirst().continuation.resume() } }
        pump()
    }
    private func fail(_ error: Error, notify: Bool) {
        trace("failed: \(error.localizedDescription)")
        let callback = notify ? onDisconnect : nil
        let reported = pairingError(error)
        close(error: reported); callback?(reported)
    }
    private func pairingError(_ error: Error) -> Error {
        let value = error as NSError
        if value.domain == CBErrorDomain {
            if value.code == CBError.peerRemovedPairingInformation.rawValue {
                return ClientError.message("系统蓝牙绑定已失效。请在 Mac 蓝牙设置中忽略此设备，再连接并完成系统配对。")
            }
            if [CBError.encryptionTimedOut.rawValue, CBError.operationCancelled.rawValue].contains(value.code) {
                return ClientError.message("系统蓝牙配对未完成。请重新连接，在 macOS 弹窗中输入设备 BLUETOOTH PIN 页面上的六位码。")
            }
        }
        if value.domain == CBATTErrorDomain,
           [CBATTError.insufficientAuthentication.rawValue, CBATTError.insufficientEncryption.rawValue].contains(value.code) {
            return ClientError.message("系统蓝牙配对未完成，请重新连接并完成 macOS 配对提示。")
        }
        return error
    }
    func close(error: Error = CancellationError()) {
        if let peripheral {
            self.peripheral = nil; peripheral.delegate = nil
            if peripheral.state != .disconnected { expectedDisconnects.insert(peripheral.identifier); manager.cancelPeripheralConnection(peripheral) }
        }
        control = nil; audio = nil; writing = false; controlFragments.reset(); audioFragments.reset(); snapshots.reset()
        openTimeout?.cancel(); openTimeout = nil; opening?.resume(throwing: error); opening = nil
        let pending = requests; requests.removeAll(); for value in pending.values { value.resume(throwing: error) }
        for timer in requestTimers.values { timer.cancel() }; requestTimers.removeAll()
        let jobs = writes; writes.removeAll(); for job in jobs { job.continuation.resume(throwing: error) }
    }
}
