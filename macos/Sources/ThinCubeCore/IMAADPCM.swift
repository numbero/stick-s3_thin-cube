import Foundation

enum IMAADPCM {
    private static let steps = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
    private static let adjust = [-1,-1,-1,-1,2,4,6,8]
    static func decode160(_ data: Data) -> [Int16]? {
        let b = [UInt8](data)
        guard b.count == 84, b[2] <= 88, b[3] == 0, b[83] & 0xf0 == 0 else { return nil }
        var predictor = Int(Int16(bitPattern: UInt16(b[0]) | UInt16(b[1]) << 8)), index = Int(b[2])
        var samples = [Int16(predictor)]; samples.reserveCapacity(160)
        for i in 0..<159 {
            let code = Int(b[4 + i/2] >> ((i & 1)*4)) & 15, step = steps[index]
            var difference = step >> 3
            if code & 4 != 0 { difference += step }
            if code & 2 != 0 { difference += step >> 1 }
            if code & 1 != 0 { difference += step >> 2 }
            predictor = max(-32768, min(32767, predictor + (code & 8 != 0 ? -difference : difference)))
            index = max(0, min(88, index + adjust[code & 7])); samples.append(Int16(predictor))
        }
        return samples
    }
}
