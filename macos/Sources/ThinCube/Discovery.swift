import Foundation
import Darwin
import Combine

struct FoundDevice: Identifiable, Equatable {
    let id: String, host: String
}
@MainActor final class Discovery: NSObject, ObservableObject, @preconcurrency NetServiceBrowserDelegate, @preconcurrency NetServiceDelegate {
    @Published var devices: [FoundDevice] = []
    let ble = BLEConnection()
    private var bleObserver: AnyCancellable?
    private var wifiDevices: [FoundDevice] = []
    private func publish() { devices = wifiDevices + ble.devices }
    private let browser = NetServiceBrowser()
    private var services: [NetService] = []
    override init() {
        super.init(); browser.delegate = self
        bleObserver = ble.$devices.sink { [weak self] values in guard let self else { return }; self.devices = self.wifiDevices + values }
        browser.searchForServices(ofType: "_thincube._tcp.", inDomain: "local.")
    }
    func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool) {
        services.append(service); service.delegate = self; service.resolve(withTimeout: 5)
    }
    func netServiceBrowser(_ browser: NetServiceBrowser, didRemove service: NetService, moreComing: Bool) {
        services.removeAll { $0 == service }; wifiDevices.removeAll { $0.id == service.name }; publish()
    }
    func netServiceDidResolveAddress(_ sender: NetService) {
        // Prefer numeric IPv4 so HTTP and the device UDP return path use the same LAN family.
        let host = sender.addresses?.compactMap { data -> String? in
            data.withUnsafeBytes { raw -> String? in
                guard let base = raw.baseAddress, raw.count >= MemoryLayout<sockaddr_in>.size,
                      base.assumingMemoryBound(to: sockaddr.self).pointee.sa_family == AF_INET else { return nil }
                var addr = base.assumingMemoryBound(to: sockaddr_in.self).pointee.sin_addr
                var text = [CChar](repeating: 0, count: Int(INET_ADDRSTRLEN))
                guard inet_ntop(AF_INET, &addr, &text, socklen_t(text.count)) != nil else { return nil }
                return String(cString: text)
            }
        }.first ?? sender.hostName
        guard let host, sender.port == 80 else { return }
        wifiDevices.removeAll { $0.id == sender.name }; wifiDevices.append(FoundDevice(id: sender.name, host: host)); publish()
    }
    func refresh(id: String?) {
        guard let id else { return }
        for service in services where service.name == id { service.stop(); service.resolve(withTimeout: 5) }
    }
    deinit { browser.stop() }
}
