import SwiftUI

struct HelpParagraph: View {
    let title: String
    let text: String
    init(_ title: String, _ text: String) { self.title = title; self.text = text }
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title).fontWeight(.semibold)
            Text(text).foregroundStyle(.secondary)
        }.fixedSize(horizontal: false, vertical: true)
    }
}

struct HelpSectionHeader<Content: View>: View {
    let title: String
    let helpTitle: String
    let content: Content
    init(_ title: String, helpTitle: String, @ViewBuilder content: () -> Content) {
        self.title = title; self.helpTitle = helpTitle; self.content = content()
    }
    var body: some View {
        HStack {
            Text(title)
            Spacer()
            HelpPopover(title: helpTitle) { content }
        }
    }
}

struct HelpPopover<Content: View>: View {
    let title: String
    let content: Content
    @State private var presented = false
    init(title: String, @ViewBuilder content: () -> Content) {
        self.title = title; self.content = content()
    }
    var body: some View {
        Button { presented.toggle() } label: {
            Image(systemName: "questionmark.circle").font(.system(size: 14))
                .frame(width: 24, height: 24).contentShape(Rectangle())
        }.buttonStyle(.plain).foregroundStyle(.secondary)
            .help(title).accessibilityLabel(title)
            .popover(isPresented: $presented, arrowEdge: .top) {
                VStack(alignment: .leading, spacing: 14) {
                    HStack {
                        Text(title).font(.headline)
                        Spacer()
                        Button { presented = false } label: { Image(systemName: "xmark") }
                            .buttonStyle(.plain).accessibilityLabel("关闭" + title)
                    }
                    Divider()
                    content.font(.callout)
                }.padding(18).frame(width: 350).textCase(nil)
            }
    }
}
