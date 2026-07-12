# Security audit

Audit date: 2026-07-13  
Audited version: 1.1.1

## Scope

The complete C source, headers, Makefile, CI workflow, linked libraries, release
binary imports, terminal lifecycle, and sensor resource ownership were reviewed.

## Automated checks

- Clang warnings: `-Wall -Wextra -Wpedantic -Werror`
- Clang static analyzer across all production sources
- AddressSanitizer and UndefinedBehaviorSanitizer
- macOS `leaks` against live sensor sampling: zero leaks reported
- PIE, stack-canary, fortified-memory, entitlement, linkage, and undefined-symbol inspection
- Source and binary scans for networking, shell/process execution, dynamic loading,
  credentials, persistence, and embedded URLs
- GitHub CI action pinned to an immutable commit with read-only repository permission

## Capability inventory

9top performs these operations:

- read temperature keys through the AppleSMC IOKit user client;
- read GPU utilization and battery temperature from IOKit registry properties;
- read CPU and memory counters through Mach host APIs;
- read swap through `sysctl` and load averages through libc;
- read terminal input and write ANSI output to the controlling terminal.

The release binary imports no networking, process creation, shell execution,
dynamic library loading, or filesystem file-operation APIs.

## Hardening

- position-independent executable;
- stack protector enabled;
- libc fortification enabled;
- dead code stripped;
- SMC enumeration bounded to 16,384 keys;
- finite and range validation on external numeric values;
- bounded formatting and terminal-width handling;
- explicit release of IOKit, CoreFoundation, Mach-port, SMC, and heap resources;
- `sigaction` handlers perform only an async-signal-safe atomic flag write;
- terminal state restored on normal exit, Ctrl-C, SIGTERM, and SIGHUP.

## Residual risks

No audit can prove that software is free of every defect. In particular:

- AppleSMC is a private, undocumented interface and may change on future macOS
  or Apple Silicon versions.
- Release binaries are ad-hoc signed, not Apple Developer ID notarized.
- The program is not confined by App Sandbox and inherits the user's normal
  permissions, even though the implementation does not use them for unrelated work.
- IOKit and operating-system framework defects are outside this repository's control.

Security reports should follow [SECURITY.md](../SECURITY.md).
