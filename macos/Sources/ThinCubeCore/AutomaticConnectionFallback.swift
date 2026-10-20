import Foundation

/// A newly received BLE advertisement is evidence that the selected device is
/// in BLE mode. Bonjour resolution alone can come from the system's old cache.
public struct AutomaticConnectionFallback: Sendable {
    private var lastRedirect: (deviceID: String, time: TimeInterval)?
    private var failedWiFi: (endpoint: ConnectionEndpoint, time: TimeInterval)?
    public init() {}
    public mutating func reset() { lastRedirect = nil; failedWiFi = nil }

    public mutating func wifiFailed(target: ConnectionTarget, now: TimeInterval) {
        guard let id = target.deviceID, !target.host.isEmpty,
              !target.host.hasPrefix("ble://"), now.isFinite else { return }
        failedWiFi = (ConnectionEndpoint(deviceID: id, host: target.host), now)
    }

    /// Do not reselect the same failed cached IP over the alternate saved route.
    /// A changed IP or a later retry remains eligible, without persistent bans.
    public func accepts(_ endpoint: ConnectionEndpoint, now: TimeInterval) -> Bool {
        guard let failedWiFi, failedWiFi.endpoint == endpoint else { return true }
        return now - failedWiFi.time >= 10
    }

    public mutating func redirect(mode: ConnectionMode, target: ConnectionTarget,
                                  advertised: ConnectionEndpoint, stage: ConnectionStage,
                                  now: TimeInterval) -> Bool {
        guard mode == .automatic, stage == .channel,
              let id = target.deviceID, id == advertised.deviceID,
              !target.host.isEmpty, !target.host.hasPrefix("ble://"),
              advertised.host.hasPrefix("ble://"), now.isFinite else { return false }
        // A failed BLE attempt must not let repeated advertisements continually
        // cancel the next Wi-Fi probe. This throttle is independent of Bonjour.
        if let lastRedirect, lastRedirect.deviceID == id, now - lastRedirect.time < 3 { return false }
        lastRedirect = (id, now)
        wifiFailed(target: target, now: now)
        return true
    }
}
