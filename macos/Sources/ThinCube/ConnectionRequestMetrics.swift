import Foundation

struct ConnectionRequestTiming: Encodable, Sendable {
    let attempt: UUID
    let event = "http"
    let endpoint: String
    let dnsMs, tcpMs, requestMs, waitMs, responseMs, totalMs: Double?
    let reusedConnection: Bool
}

/// Collect only durations; never retain URLs, addresses, headers or payloads.
final class ConnectionRequestMetrics: NSObject, URLSessionTaskDelegate, @unchecked Sendable {
    private let attempt: UUID, endpoint: String
    private let completed: @Sendable (ConnectionRequestTiming) -> Void
    init(attempt: UUID, endpoint: String, completed: @escaping @Sendable (ConnectionRequestTiming) -> Void) {
        self.attempt = attempt; self.endpoint = endpoint; self.completed = completed
    }
    func urlSession(_ session: URLSession, task: URLSessionTask, didFinishCollecting metrics: URLSessionTaskMetrics) {
        func milliseconds(_ start: Date?, _ end: Date?) -> Double? {
            guard let start, let end else { return nil }
            return max(0, end.timeIntervalSince(start)*1000)
        }
        for value in metrics.transactionMetrics {
            completed(ConnectionRequestTiming(attempt: attempt, endpoint: endpoint,
                dnsMs: milliseconds(value.domainLookupStartDate, value.domainLookupEndDate),
                tcpMs: milliseconds(value.connectStartDate, value.connectEndDate),
                requestMs: milliseconds(value.requestStartDate, value.requestEndDate),
                waitMs: milliseconds(value.requestEndDate, value.responseStartDate),
                responseMs: milliseconds(value.responseStartDate, value.responseEndDate),
                totalMs: milliseconds(value.fetchStartDate, value.responseEndDate),
                reusedConnection: value.isReusedConnection))
        }
    }
}
