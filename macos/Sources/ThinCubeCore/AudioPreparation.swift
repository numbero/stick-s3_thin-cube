import Foundation

/// Preparation may finish after cancellation. Only the current operation may
/// publish readiness, bind a receiver, or clear its in-flight state.
@MainActor public final class AudioPreparation {
    private var task: Task<Void, Never>?
    private var token: UUID?
    public var busy: Bool { task != nil }
    public init() {}
    public func start(prepare: @escaping () async throws -> Void,
                      ready: @escaping () throws -> Void,
                      failed: @escaping (Error) -> Void) {
        guard task == nil else { return }
        let id = UUID(); token = id
        task = Task { [weak self] in
            do {
                try Task.checkCancellation()
                try await prepare()
                guard let self, self.token == id, !Task.isCancelled else { return }
                try ready()
                if self.token == id { self.task = nil; self.token = nil }
            } catch {
                guard let self, self.token == id, !Task.isCancelled else { return }
                self.task = nil; self.token = nil
                failed(error)
            }
        }
    }
    public func cancel() { token = nil; task?.cancel(); task = nil }
}
