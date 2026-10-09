import AppKit
import SwiftUI

/// One native slider configuration for brightness, gain and sound volume.
/// Keeping quantization separate from tick marks avoids an untinted track on macOS.
struct SettingsSliderRow: View {
    @Environment(\.isEnabled) private var isEnabled
    let title: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let step: Double
    let valueLabel: String
    var save: (() -> Void)? = nil

    var body: some View {
        HStack(spacing: 12) {
            Text(title).frame(width: 72, alignment: .leading)
            SettingsSlider(value: $value, range: range, step: step, title: title, valueLabel: valueLabel)
                .frame(minWidth: 120, maxWidth: .infinity)
                .frame(height: 24)
                .accessibilityRepresentation {
                    Slider(value: Binding(get: { value }, set: {
                        guard isEnabled else { return }
                        value = SliderStep.quantized($0, in: range, step: step)
                    }), in: range, step: step)
                        .accessibilityLabel(title)
                        .accessibilityValue(valueLabel)
                }
                .accessibilityAdjustableAction { direction in
                    guard isEnabled else { return }
                    switch direction {
                    case .increment: value = SliderStep.quantized(value + step, in: range, step: step)
                    case .decrement: value = SliderStep.quantized(value - step, in: range, step: step)
                    @unknown default: break
                    }
                }
            Text(valueLabel).monospacedDigit().frame(width: 52, alignment: .trailing)
            if let save {
                Button("保存", action: save).frame(width: 58)
            } else {
                Text("自动保存").font(.caption).foregroundStyle(.secondary).frame(width: 58)
            }
        }
    }
}

private struct SettingsSlider: NSViewRepresentable {
    @Binding var value: Double
    let range: ClosedRange<Double>
    let step: Double
    let title: String
    let valueLabel: String

    func makeCoordinator() -> Coordinator { Coordinator(value: $value) }

    func makeNSView(context: Context) -> SteppedSlider {
        let slider = SteppedSlider()
        slider.sliderType = .linear
        slider.isVertical = false
        slider.numberOfTickMarks = 0
        slider.isContinuous = true
        slider.controlSize = .regular
        slider.trackFillColor = NSColor(srgbRed: 0.973, green: 0.690, blue: 0, alpha: 1)
        if #available(macOS 26.0, *) { slider.tintProminence = .primary }
        slider.setContentHuggingPriority(.defaultLow, for: .horizontal)
        slider.target = context.coordinator
        slider.action = #selector(Coordinator.changed(_:))
        return slider
    }

    func updateNSView(_ slider: SteppedSlider, context: Context) {
        context.coordinator.value = $value
        slider.minValue = range.lowerBound
        slider.maxValue = range.upperBound
        slider.step = step
        slider.doubleValue = value
        slider.isEnabled = context.environment.isEnabled
        slider.setAccessibilityLabel(title)
        slider.setAccessibilityValueDescription(valueLabel)
        slider.toolTip = valueLabel
    }

    final class Coordinator: NSObject {
        var value: Binding<Double>
        init(value: Binding<Double>) { self.value = value }

        @objc func changed(_ sender: SteppedSlider) {
            guard sender.isEnabled else { sender.doubleValue = value.wrappedValue; return }
            let next = sender.quantized(sender.doubleValue)
            sender.doubleValue = next
            if value.wrappedValue != next { value.wrappedValue = next }
        }
    }
}

private final class SteppedSlider: NSSlider {
    var step = 1.0

    func quantized(_ value: Double) -> Double {
        SliderStep.quantized(value, in: minValue...maxValue, step: step)
    }

    private func adjust(by steps: Double) -> Bool {
        guard isEnabled else { return false }
        doubleValue = quantized(doubleValue + step * steps)
        sendAction(action, to: target)
        return true
    }

    override func keyDown(with event: NSEvent) {
        // A single arrow always moves by the same step as dragging and VoiceOver.
        let amount = event.modifierFlags.contains(.option) ? 10.0 : 1.0
        switch event.keyCode {
        case 123, 125: _ = adjust(by: -amount)
        case 124, 126: _ = adjust(by: amount)
        case 115:
            guard isEnabled else { return }
            doubleValue = minValue; sendAction(action, to: target)
        case 119:
            guard isEnabled else { return }
            doubleValue = maxValue; sendAction(action, to: target)
        default: super.keyDown(with: event)
        }
    }

    override func accessibilityPerformIncrement() -> Bool { adjust(by: 1) }
    override func accessibilityPerformDecrement() -> Bool { adjust(by: -1) }

    override func isAccessibilitySelectorAllowed(_ selector: Selector) -> Bool {
        if selector == #selector(accessibilityPerformIncrement) { return isEnabled && doubleValue < maxValue }
        if selector == #selector(accessibilityPerformDecrement) { return isEnabled && doubleValue > minValue }
        return super.isAccessibilitySelectorAllowed(selector)
    }
}

private enum SliderStep {
    static func quantized(_ value: Double, in range: ClosedRange<Double>, step: Double) -> Double {
        let steps = ((value - range.lowerBound) / step).rounded()
        // These settings use integer or tenth-unit steps. Remove arithmetic
        // noise so a saved value compares equal to the next device snapshot.
        let normalized = ((range.lowerBound + steps * step) * 1_000_000_000).rounded() / 1_000_000_000
        return min(max(normalized, range.lowerBound), range.upperBound)
    }
}
