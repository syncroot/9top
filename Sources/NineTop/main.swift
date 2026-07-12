import NativeCore
import Darwin
import Foundation

struct Snapshot {
    var cpuTemp: Double?
    var gpuTemp: Double?
    var batteryTemp: Double?
    var cpuUsage: Double?
    var gpuUsage: Double?
    var memoryUsed: UInt64
    var memoryTotal: UInt64
    var swapUsed: UInt64
    var swapTotal: UInt64
    var load: (Double, Double, Double)
}

func valid(_ value: Double) -> Double? { value.isFinite ? value : nil }

func readSnapshot(includeBattery: Bool, previousBattery: Double?) -> Snapshot {
    var raw = nine_top_metrics()
    _ = nine_top_read_metrics(&raw, includeBattery ? 1 : 0)
    var loads = [Double](repeating: 0, count: 3)
    _ = getloadavg(&loads, 3)
    return Snapshot(
        cpuTemp: valid(raw.cpu_temperature),
        gpuTemp: valid(raw.gpu_temperature),
        batteryTemp: includeBattery ? valid(raw.battery_temperature) : previousBattery,
        cpuUsage: valid(raw.cpu_usage),
        gpuUsage: valid(raw.gpu_usage),
        memoryUsed: raw.memory_used,
        memoryTotal: raw.memory_total,
        swapUsed: raw.swap_used,
        swapTotal: raw.swap_total,
        load: (loads[0], loads[1], loads[2])
    )
}

func format(_ value: Double?, decimals: Int = 1) -> String {
    guard let value else { return "--" }
    return String(format: "%.*f", decimals, value)
}

func percent(_ used: UInt64, _ total: UInt64) -> Double? {
    guard total > 0 else { return nil }
    return min(100, max(0, Double(used) / Double(total) * 100))
}

func gib(_ bytes: UInt64) -> String {
    String(format: "%.1f", Double(bytes) / 1_073_741_824)
}

func meter(label: String, value: Double?, maximum: Double, width: Int, suffix: String) -> String {
    let available = max(4, width - 28)
    let ratio = value.map { min(1, max(0, $0 / maximum)) } ?? 0
    let filled = Int((Double(available) * ratio).rounded())
    let bar = String(repeating: "|", count: filled) + String(repeating: ".", count: available - filled)
    let paddedLabel = label.padding(toLength: 11, withPad: " ", startingAt: 0)
    return "  \(paddedLabel) [\(bar)] \(format(value).leftPadded(to: 6)) \(suffix)"
}

extension String {
    func leftPadded(to width: Int) -> String {
        String(repeating: " ", count: max(0, width - count)) + self
    }
}

func clipped(_ value: String, width: Int) -> String {
    guard width > 0 else { return "" }
    let prefix = String(value.prefix(width))
    return prefix + String(repeating: " ", count: max(0, width - prefix.count))
}

enum ANSI {
    static let reset = "\u{1B}[0m"
    static let title = "\u{1B}[30;46;1m"
    static let cyan = "\u{1B}[96;1m"
    static let green = "\u{1B}[92m"
    static let yellow = "\u{1B}[93;1m"
    static let red = "\u{1B}[91;1m"
    static let muted = "\u{1B}[90m"
    static let footer = "\u{1B}[30;42m"
}

struct StyledLine {
    let text: String
    let color: String

    init(_ text: String, _ color: String = "") {
        self.text = text
        self.color = color
    }
}

func metricColor(_ value: Double?, warning: Double, critical: Double) -> String {
    guard let value else { return ANSI.muted }
    if value >= critical { return ANSI.red }
    if value >= warning { return ANSI.yellow }
    return ANSI.green
}

func render(_ snapshot: Snapshot, width: Int, height: Int, paused: Bool, colors: Bool = false) -> String {
    guard width >= 20, height >= 5 else {
        return "\u{1B}[H" + clipped("9top: terminal too small", width: width)
    }
    var lines = [StyledLine]()
    let state = paused ? "PAUSED" : "LIVE"
    let clock = DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .medium)
    lines.append(StyledLine(" 9top  \(state)  \(clock)", ANSI.title))

    if width < 64 || height < 16 {
        let cpuColor = metricColor(
            max(snapshot.cpuTemp ?? 0, snapshot.cpuUsage ?? 0), warning: 70, critical: 85)
        lines.append(StyledLine(" CPU  \(format(snapshot.cpuTemp)) C   load \(format(snapshot.cpuUsage, decimals: 0))%", cpuColor))
        let gpuColor = metricColor(
            max(snapshot.gpuTemp ?? 0, snapshot.gpuUsage ?? 0), warning: 70, critical: 85)
        lines.append(StyledLine(" GPU  \(format(snapshot.gpuTemp)) C   load \(format(snapshot.gpuUsage, decimals: 0))%", gpuColor))
        lines.append(StyledLine(" BAT  \(format(snapshot.batteryTemp)) C", metricColor(snapshot.batteryTemp, warning: 35, critical: 40)))
        if lines.count < height - 1 {
            let memory = percent(snapshot.memoryUsed, snapshot.memoryTotal)
            lines.append(StyledLine(" MEM  \(format(memory, decimals: 0))%  \(gib(snapshot.memoryUsed))/\(gib(snapshot.memoryTotal)) GiB", metricColor(memory, warning: 75, critical: 90)))
        }
        if lines.count < height - 1 {
            lines.append(StyledLine(String(format: " LOAD %.2f %.2f %.2f", snapshot.load.0, snapshot.load.1, snapshot.load.2)))
        }
    } else {
        lines.append(StyledLine(" THERMALS " + String(repeating: "-", count: max(0, width - 11)), ANSI.cyan))
        lines.append(StyledLine(meter(label: "CPU average", value: snapshot.cpuTemp, maximum: 105, width: width, suffix: "C"), metricColor(snapshot.cpuTemp, warning: 70, critical: 85)))
        lines.append(StyledLine(meter(label: "GPU average", value: snapshot.gpuTemp, maximum: 105, width: width, suffix: "C"), metricColor(snapshot.gpuTemp, warning: 70, critical: 85)))
        lines.append(StyledLine(meter(label: "Battery", value: snapshot.batteryTemp, maximum: 50, width: width, suffix: "C"), metricColor(snapshot.batteryTemp, warning: 35, critical: 40)))
        lines.append(StyledLine(""))
        lines.append(StyledLine(" SYSTEM " + String(repeating: "-", count: max(0, width - 9)), ANSI.cyan))
        lines.append(StyledLine(meter(label: "CPU load", value: snapshot.cpuUsage, maximum: 100, width: width, suffix: "%"), metricColor(snapshot.cpuUsage, warning: 70, critical: 90)))
        lines.append(StyledLine(meter(label: "GPU load", value: snapshot.gpuUsage, maximum: 100, width: width, suffix: "%"), metricColor(snapshot.gpuUsage, warning: 70, critical: 90)))
        let memory = percent(snapshot.memoryUsed, snapshot.memoryTotal)
        lines.append(StyledLine(meter(label: "Memory", value: memory, maximum: 100, width: width, suffix: "%"), metricColor(memory, warning: 75, critical: 90)))
        lines.append(StyledLine("  Memory      \(gib(snapshot.memoryUsed)) / \(gib(snapshot.memoryTotal)) GiB", ANSI.muted))
        let swap = percent(snapshot.swapUsed, snapshot.swapTotal)
        lines.append(StyledLine(meter(label: "Swap", value: swap, maximum: 100, width: width, suffix: "%"), metricColor(swap, warning: 25, critical: 60)))
        lines.append(StyledLine("  Load avg    \(String(format: "%.2f  %.2f  %.2f", snapshot.load.0, snapshot.load.1, snapshot.load.2))"))
    }

    while lines.count < height - 1 { lines.append(StyledLine("")) }
    if lines.count >= height { lines = Array(lines.prefix(height - 1)) }
    lines.append(StyledLine(" q quit   p pause", ANSI.footer))
    // cfmakeraw disables ONLCR, so each row must explicitly return to column 0.
    let frame = lines.prefix(height).map { line -> String in
        let text = clipped(line.text, width: width)
        return colors && !line.color.isEmpty ? line.color + text + ANSI.reset : text
    }
    return "\u{1B}[H" + frame.joined(separator: "\r\n")
}

struct Terminal {
    private var original = termios()

    mutating func enter() {
        tcgetattr(STDIN_FILENO, &original)
        var raw = original
        cfmakeraw(&raw)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)
        print("\u{1B}[?1049h\u{1B}[?25l\u{1B}[2J", terminator: "")
        fflush(stdout)
    }

    mutating func leave() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original)
        print("\u{1B}[?25h\u{1B}[?1049l", terminator: "")
        fflush(stdout)
    }

    func size() -> (width: Int, height: Int) {
        var value = winsize()
        guard ioctl(STDOUT_FILENO, TIOCGWINSZ, &value) == 0 else { return (80, 24) }
        return (max(1, Int(value.ws_col)), max(1, Int(value.ws_row)))
    }
}

func intervalArgument() -> Double {
    let arguments = CommandLine.arguments
    if arguments.contains("--version") {
        print("9top 1.0.0")
        exit(0)
    }
    if let index = arguments.firstIndex(where: { $0 == "--interval" || $0 == "-i" }), index + 1 < arguments.count,
       let value = Double(arguments[index + 1]), value >= 1 {
        return value
    }
    return 2.0
}

let sampleInterval = intervalArgument()
let colorsEnabled = ProcessInfo.processInfo.environment["NO_COLOR"] == nil
    && ProcessInfo.processInfo.environment["TERM"] != "dumb"
var terminal = Terminal()
nine_top_install_signal_handlers()
terminal.enter()
defer { terminal.leave() }

var snapshot = readSnapshot(includeBattery: true, previousBattery: nil)
var paused = false
var lastSample = ContinuousClock.now
var lastBattery = ContinuousClock.now
var lastSize = (width: 0, height: 0)
var needsRender = true

while true {
    if nine_top_should_terminate() != 0 { break }
    let size = terminal.size()
    if size != lastSize { lastSize = size; needsRender = true }

    let now = ContinuousClock.now
    if !paused && now - lastSample >= .seconds(sampleInterval) {
        let includeBattery = now - lastBattery >= .seconds(30)
        snapshot = readSnapshot(includeBattery: includeBattery, previousBattery: snapshot.batteryTemp)
        lastSample = now
        if includeBattery { lastBattery = now }
        needsRender = true
    }
    if needsRender {
        print(render(snapshot, width: size.width, height: size.height, paused: paused, colors: colorsEnabled), terminator: "")
        fflush(stdout)
        needsRender = false
    }

    var descriptor = pollfd(fd: STDIN_FILENO, events: Int16(POLLIN), revents: 0)
    if poll(&descriptor, 1, 200) > 0 && descriptor.revents & Int16(POLLIN) != 0 {
        var byte: UInt8 = 0
        if read(STDIN_FILENO, &byte, 1) == 1 {
            if byte == Character("q").asciiValue || byte == 3 { break }
            if byte == Character("p").asciiValue || byte == 32 {
                paused.toggle()
                needsRender = true
            }
        }
    }
}
