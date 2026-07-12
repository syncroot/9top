import Testing
@testable import NineTop

@Test func percentagesAreSafe() {
    #expect(percent(4, 8) == 50)
    #expect(percent(1, 0) == nil)
}

@Test func clippingNeverExceedsWidth() {
    #expect(clipped("abcdefgh", width: 4) == "abcd")
    #expect(clipped("ab", width: 4) == "ab  ")
}

@Test func framesUseRawTerminalLineEndings() {
    let snapshot = Snapshot(
        cpuTemp: 40, gpuTemp: 39, batteryTemp: 30, cpuUsage: 10, gpuUsage: 5,
        memoryUsed: 4, memoryTotal: 8, swapUsed: 0, swapTotal: 0,
        load: (1, 1, 1)
    )
    let frame = render(snapshot, width: 40, height: 10, paused: false)
    #expect(frame.contains("\r\n"))
    #expect(!frame.replacing("\r\n", with: "").contains("\n"))
}

@Test func ansiColorsDoNotReplaceSemanticValues() {
    let snapshot = Snapshot(
        cpuTemp: 90, gpuTemp: 75, batteryTemp: 30, cpuUsage: 95, gpuUsage: 80,
        memoryUsed: 95, memoryTotal: 100, swapUsed: 0, swapTotal: 0,
        load: (1, 1, 1)
    )
    let frame = render(snapshot, width: 80, height: 20, paused: false, colors: true)
    #expect(frame.contains(ANSI.red))
    #expect(frame.contains("90.0 C"))
    #expect(frame.contains("95.0 %"))
}
