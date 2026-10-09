import Foundation
import AudioToolbox
import CoreAudio
import ThinCubeCore

final class AudioBridge {
    let buffer: JitterBuffer
    private var unit: AudioUnit?
    private(set) var device = AudioDeviceID(0)
    init(buffer: JitterBuffer) { self.buffer = buffer }
    private func check(_ status: OSStatus) throws { if status != noErr { throw ClientError.message("音频设备错误：\(status)") } }
    func start() throws {
        stop()
        var property = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDevices, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
        var size: UInt32 = 0
        try check(AudioObjectGetPropertyDataSize(AudioObjectID(kAudioObjectSystemObject), &property, 0, nil, &size))
        var devices = [AudioDeviceID](repeating: 0, count: Int(size)/MemoryLayout<AudioDeviceID>.size)
        try check(AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &property, 0, nil, &size, &devices))
        device = devices.first { id in
            var p = AudioObjectPropertyAddress(mSelector: kAudioDevicePropertyDeviceUID, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
            var name: CFString?; var count = UInt32(MemoryLayout<CFString?>.size)
            let result = withUnsafeMutablePointer(to: &name) { pointer in
                AudioObjectGetPropertyData(id, &p, 0, nil, &count, pointer)
            }
            return result == noErr && (name as String?)?.contains("BlackHole2ch") == true
        } ?? 0
        guard device != 0 else { throw ClientError.message("请先安装 BlackHole 2ch，再点击“准备音频”。") }
        var rate = Float64(48000)
        property = AudioObjectPropertyAddress(mSelector: kAudioDevicePropertyNominalSampleRate, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
        try check(AudioObjectSetPropertyData(device, &property, 0, nil, UInt32(MemoryLayout<Float64>.size), &rate))
        var desc = AudioComponentDescription(componentType: kAudioUnitType_Output, componentSubType: kAudioUnitSubType_HALOutput, componentManufacturer: kAudioUnitManufacturer_Apple, componentFlags: 0, componentFlagsMask: 0)
        guard let component = AudioComponentFindNext(nil, &desc) else { throw ClientError.message("无法加载 Core Audio") }
        var newUnit: AudioUnit?
        try check(AudioComponentInstanceNew(component, &newUnit))
        guard let audioUnit = newUnit else { throw ClientError.message("无法创建音频输出") }
        unit = audioUnit
        do {
            var off: UInt32 = 0, on: UInt32 = 1
            try check(AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &on, 4))
            try check(AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &off, 4))
            try check(AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, 4))
            var format = AudioStreamBasicDescription(mSampleRate: 48000, mFormatID: kAudioFormatLinearPCM, mFormatFlags: kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved, mBytesPerPacket: 4, mFramesPerPacket: 1, mBytesPerFrame: 4, mChannelsPerFrame: 2, mBitsPerChannel: 32, mReserved: 0)
            try check(AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, UInt32(MemoryLayout.size(ofValue: format))))
            var callback = AURenderCallbackStruct(inputProc: { ref, _, _, _, count, io in
                guard let io else { return noErr }
                let bridge = Unmanaged<AudioBridge>.fromOpaque(ref).takeUnretainedValue()
                let buffers = UnsafeMutableAudioBufferListPointer(io)
                guard let first = buffers.first?.mData else { return noErr }
                let output = first.assumingMemoryBound(to: Float.self)
                bridge.buffer.render(into: output, count: Int(count))
                for buffer in buffers.dropFirst() { if let data = buffer.mData { memcpy(data, output, min(Int(buffer.mDataByteSize), Int(count)*4)) } }
                return noErr
            }, inputProcRefCon: Unmanaged.passUnretained(self).toOpaque())
            try check(AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, UInt32(MemoryLayout.size(ofValue: callback))))
            try check(AudioUnitInitialize(audioUnit)); try check(AudioOutputUnitStart(audioUnit))
        } catch { stop(); throw error }
    }
    func selectSystemInput() throws {
        guard device != 0 else { throw ClientError.message("请先准备音频") }
        var p = AudioObjectPropertyAddress(mSelector: kAudioHardwarePropertyDefaultInputDevice, mScope: kAudioObjectPropertyScopeGlobal, mElement: kAudioObjectPropertyElementMain)
        try check(AudioObjectSetPropertyData(AudioObjectID(kAudioObjectSystemObject), &p, 0, nil, 4, &device))
    }
    func stop() {
        if let unit { AudioOutputUnitStop(unit); AudioUnitUninitialize(unit); AudioComponentInstanceDispose(unit) }
        unit = nil; device = 0; buffer.setSession(nil)
    }
    deinit { stop() }
}
