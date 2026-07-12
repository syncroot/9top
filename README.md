# 9top

`9top` is a lightweight, full-screen thermal and system monitor for Apple
Silicon Macs. It is a single pure-C executable with no third-party runtime
dependencies, helper processes, sudo requirement, analytics, or network calls.

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

Download and verify the binary from the latest GitHub release:

```sh
curl -fLO https://github.com/syncroot/9top/releases/latest/download/9top-arm64
curl -fLO https://github.com/syncroot/9top/releases/latest/download/SHA256SUMS
shasum -a 256 -c SHA256SUMS
mkdir -p ~/.local/bin
install -m 755 9top-arm64 ~/.local/bin/9top
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
make test
make release
```

The binary is generated at `build/9top`. Build artifacts are ignored and can
be removed with `make clean`.

## Resource profile

9top uses event-driven rendering, retains no metric history, and launches no
helper processes. Resource usage varies by hardware and macOS version; measured
figures for each release are published in its release notes.

## Private API notice

Apple does not publish a supported API for CPU and GPU die temperatures. 9top
reads them through the AppleSMC user-client interface and reads GPU utilization
from the IOKit accelerator registry. Major macOS or hardware changes may require
maintenance.

## Privacy

All metrics remain on the Mac. 9top does not connect to the network, write
monitoring data to disk, or collect analytics.

See [SECURITY.md](SECURITY.md) for vulnerability reporting and security
boundaries. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for upstream
attribution.

## License

MIT © 2026 Mohammed
