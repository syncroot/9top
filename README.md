# 9top

`9top` is a lightweight, full-screen thermal and system monitor for Apple
Silicon Macs. It is a single native executable with no runtime dependencies,
helper processes, sudo requirement, analytics, or network access.

## Features

- CPU, GPU, and battery temperatures
- CPU and GPU utilization
- Memory, swap, and system load
- Semantic normal/elevated/high colors
- Responsive full and compact terminal layouts
- Event-driven rendering and a two-second default sample interval
- Clean terminal restoration after `q`, Ctrl-C, SIGTERM, or SIGHUP

## Requirements

- Apple Silicon Mac
- macOS 14 or newer

## Install

Download the binary from the latest GitHub release:

```sh
curl -fL https://github.com/syncroot/9top/releases/latest/download/9top-arm64 -o ~/.local/bin/9top
chmod +x ~/.local/bin/9top
```

Ensure `~/.local/bin` is in `PATH`, then run:

```sh
9top
```

## Controls

| Key | Action |
| --- | --- |
| `q` or Ctrl-C | Quit |
| `p` or Space | Pause or resume |

Use a custom sampling interval of at least one second:

```sh
9top --interval 3
```

Disable colors using the standard `NO_COLOR` variable:

```sh
NO_COLOR=1 9top
```

## Build from source

Xcode Command Line Tools are required:

```sh
swift test
swift build -c release
```

The binary is generated at `.build/release/9top`. Build artifacts are ignored
and can be removed with `swift package clean`.

## Resource profile

On the development M-series Mac at the default interval, 9top used about 0.4%
average CPU, 9 MB resident memory, and no helper processes. Actual usage varies
by hardware and macOS version.

## Private API notice

Apple does not publish a supported API for CPU and GPU die temperatures. 9top
reads them through the AppleSMC user-client interface and reads GPU utilization
from the IOKit accelerator registry. Major macOS or hardware changes may require
maintenance.

## Privacy

All metrics remain on the Mac. 9top does not connect to the network, write
monitoring data to disk, or collect analytics.

## License

MIT © 2026 Mohammed
