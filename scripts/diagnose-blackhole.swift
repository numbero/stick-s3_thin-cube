#!/usr/bin/env swift
// Read-only Core Audio snapshots. Does not open audio streams, capture samples,
// set volume/mute, or change the default device. A client list is context, not
// attribution: Core Audio property notifications do not identify the writer.
// Usage: swift scripts/diagnose-blackhole.swift [--watch-seconds 300]
import Foundation
import CoreAudio

func address(_ selector: AudioObjectPropertySelector,
             _ scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal,
             _ element: AudioObjectPropertyElement = kAudioObjectPropertyElementMain) -> AudioObjectPropertyAddress {
    AudioObjectPropertyAddress(mSelector: selector, mScope: scope, mElement: element)
}

func read<T>(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector,
             scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal,
             element: AudioObjectPropertyElement = kAudioObjectPropertyElementMain,
             initial: T) -> T? {
    var property = address(selector, scope, element)
    guard AudioObjectHasProperty(object, &property) else { return nil }
    var value = initial
    var size = UInt32(MemoryLayout<T>.size)
    let status = withUnsafeMutableBytes(of: &value) {
        AudioObjectGetPropertyData(object, &property, 0, nil, &size, $0.baseAddress!)
    }
    return status == noErr ? value : nil
}

func ids(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector,
         scope: AudioObjectPropertyScope = kAudioObjectPropertyScopeGlobal) -> [AudioObjectID] {
    var property = address(selector, scope)
    var size: UInt32 = 0
    guard AudioObjectGetPropertyDataSize(object, &property, 0, nil, &size) == noErr,
          size > 0 else { return [] }
    var values = [AudioObjectID](repeating: 0, count: Int(size) / MemoryLayout<AudioObjectID>.size)
    let status = values.withUnsafeMutableBytes {
        AudioObjectGetPropertyData(object, &property, 0, nil, &size, $0.baseAddress!)
    }
    return status == noErr ? Array(values.prefix(Int(size) / MemoryLayout<AudioObjectID>.size)) : []
}

func string(_ object: AudioObjectID, _ selector: AudioObjectPropertySelector) -> String? {
    var property = address(selector)
    var value: CFString?
    var size = UInt32(MemoryLayout<CFString?>.size)
    let status = withUnsafeMutablePointer(to: &value) {
        AudioObjectGetPropertyData(object, &property, 0, nil, &size, $0)
    }
    return status == noErr ? value as String? : nil
}

func deviceInfo(_ id: AudioDeviceID) -> [String: Any] {
    ["id": id, "name": string(id, kAudioObjectPropertyName) ?? "unknown",
     "uid": string(id, kAudioDevicePropertyDeviceUID) ?? "unknown"]
}

func snapshot() -> [String: Any] {
    let system = AudioObjectID(kAudioObjectSystemObject)
    let devices = ids(system, kAudioHardwarePropertyDevices)
    var result: [String: Any] = [:]
    if devices.isEmpty {
        result["warning"] = "No Core Audio devices are visible; check sandbox/service access before interpreting this snapshot."
    }
    for (name, selector) in [("defaultInput", kAudioHardwarePropertyDefaultInputDevice),
                             ("defaultOutput", kAudioHardwarePropertyDefaultOutputDevice),
                             ("systemOutput", kAudioHardwarePropertyDefaultSystemOutputDevice)] {
        if let id = read(system, selector, initial: AudioDeviceID(0)), id != 0 {
            result[name] = deviceInfo(id)
        }
    }
    let blackholes = devices.filter { string($0, kAudioDevicePropertyDeviceUID)?.contains("BlackHole2ch") == true }
    result["blackHoleDevices"] = blackholes.map { id -> [String: Any] in
        var item = deviceInfo(id)
        item["sampleRate"] = read(id, kAudioDevicePropertyNominalSampleRate, initial: Float64(0))
        item["running"] = read(id, kAudioDevicePropertyDeviceIsRunningSomewhere, initial: UInt32(0))
        for (name, scope) in [("input", kAudioObjectPropertyScopeInput), ("output", kAudioObjectPropertyScopeOutput)] {
            item[name] = (0...2).compactMap { channel -> [String: Any]? in
                let element = AudioObjectPropertyElement(channel)
                var control: [String: Any] = ["element": channel]
                control["volumeScalar"] = read(id, kAudioDevicePropertyVolumeScalar, scope: scope, element: element, initial: Float32(0))
                control["volumeDB"] = read(id, kAudioDevicePropertyVolumeDecibels, scope: scope, element: element, initial: Float32(0))
                control["mute"] = read(id, kAudioDevicePropertyMute, scope: scope, element: element, initial: UInt32(0))
                return control.count > 1 ? control : nil
            }
        }
        return item
    }
    if #available(macOS 14.2, *) {
        result["audioClients"] = ids(system, kAudioHardwarePropertyProcessObjectList).map { id -> [String: Any] in
            var item: [String: Any] = ["objectID": id]
            item["pid"] = read(id, kAudioProcessPropertyPID, initial: pid_t(0))
            item["bundleID"] = string(id, kAudioProcessPropertyBundleID)
            item["runningInput"] = read(id, kAudioProcessPropertyIsRunningInput, initial: UInt32(0))
            item["runningOutput"] = read(id, kAudioProcessPropertyIsRunningOutput, initial: UInt32(0))
            item["inputDevices"] = ids(id, kAudioProcessPropertyDevices, scope: kAudioObjectPropertyScopeInput).map(deviceInfo)
            item["outputDevices"] = ids(id, kAudioProcessPropertyDevices, scope: kAudioObjectPropertyScopeOutput).map(deviceInfo)
            return item
        }
    }
    return result
}

let args = Array(CommandLine.arguments.dropFirst())
var seconds: Double = 0
if !args.isEmpty {
    guard args.count == 2, args[0] == "--watch-seconds", let value = Double(args[1]),
          value.isFinite, (0...86400).contains(value) else {
        fputs("Usage: swift scripts/diagnose-blackhole.swift [--watch-seconds 300]\n", stderr)
        exit(2)
    }
    seconds = value
}
let clock = ISO8601DateFormatter()
clock.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
let deadline = Date().addingTimeInterval(seconds)
var previous: Data?
repeat {
    var state = snapshot()
    let comparable = try JSONSerialization.data(withJSONObject: state, options: [.sortedKeys])
    if comparable != previous {
        state["time"] = clock.string(from: Date())
        state["event"] = previous == nil ? "snapshot" : "stateChanged"
        let data = try JSONSerialization.data(withJSONObject: state, options: [.sortedKeys])
        FileHandle.standardOutput.write(data + Data([10]))
        previous = comparable
    }
    if Date() >= deadline { break }
    RunLoop.current.run(until: min(deadline, Date().addingTimeInterval(0.25)))
} while Date() < deadline
