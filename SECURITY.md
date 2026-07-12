# Security policy

## Supported versions

Only the latest 9top release receives security fixes.

## Reporting a vulnerability

Please do not open a public issue for a suspected vulnerability. Use GitHub's
private vulnerability reporting feature under **Security → Advisories → Report
a vulnerability** in the `syncroot/9top` repository.

Include the affected version, macOS and hardware version, reproduction steps,
and expected impact. Please allow reasonable time for investigation before any
public disclosure.

## Security boundaries

9top runs as the invoking user and does not request sudo or entitlements. Its
implementation contains no network, process-spawn, persistence, background
launch, dynamic-code-loading, or intentional filesystem-write operations. It
reads hardware and system counters from AppleSMC, IOKit, Mach, and sysctl.

9top is not an App Sandbox application, so the operating system does not enforce
those boundaries; like other command-line programs, it inherits the invoking
user's permissions. Release binaries are currently ad-hoc signed rather than
notarized with an Apple Developer ID. Verify the published SHA-256 checksum.
