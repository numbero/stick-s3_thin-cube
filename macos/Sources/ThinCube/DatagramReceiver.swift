import Foundation
import Darwin
import ThinCubeCore

final class DatagramReceiver {
    private var source: DispatchSourceRead?
    private let queue = DispatchQueue(label: "ThinCube.audio.network", qos: .userInteractive)
    private(set) var port: UInt16 = 0
    func start(buffer: JitterBuffer, firstPacket: @escaping @Sendable () -> Void = {}) throws {
        stop()
        let fd = socket(AF_INET, SOCK_DGRAM, 0)
        guard fd >= 0 else { throw ClientError.message("无法创建音频接收端口") }
        var addr = sockaddr_in(); addr.sin_family = sa_family_t(AF_INET); addr.sin_len = UInt8(MemoryLayout<sockaddr_in>.size)
        let bound = withUnsafePointer(to: &addr) { ptr in ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { bind(fd, $0, socklen_t(MemoryLayout<sockaddr_in>.size)) } }
        guard bound == 0 else { close(fd); throw ClientError.message("无法绑定音频接收端口") }
        var size = socklen_t(MemoryLayout<sockaddr_in>.size)
        _ = withUnsafeMutablePointer(to: &addr) { ptr in ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { getsockname(fd, $0, &size) } }
        port = UInt16(bigEndian: addr.sin_port)
        _ = fcntl(fd, F_SETFL, O_NONBLOCK)
        let source = DispatchSource.makeReadSource(fileDescriptor: fd, queue: queue)
        let initialFrames = buffer.statistics().receivedFrames
        var reportedFirstPacket = false
        source.setEventHandler {
            var bytes = [UInt8](repeating: 0, count: 2048)
            while true {
                let count = recv(fd, &bytes, bytes.count, 0)
                if count <= 0 { break }
                if count == 988 {
                    buffer.accept(Data(bytes.prefix(count)))
                    if !reportedFirstPacket, buffer.statistics().receivedFrames > initialFrames {
                        reportedFirstPacket = true; firstPacket()
                    }
                }
            }
        }
        source.setCancelHandler { close(fd) }
        self.source = source; source.resume()
    }
    func stop() { source?.cancel(); source = nil; port = 0 }
    deinit { stop() }
}
