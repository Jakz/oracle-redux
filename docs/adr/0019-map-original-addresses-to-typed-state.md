# Map original addresses to typed state

Status: Accepted

Typed C++ state is authoritative; Oracle Redux does not expose a simulated flat Game Boy memory array to Campaign Scripts. Each supported cartridge address resolves through a validated Original State Key to typed accessors that deliberately reproduce byte width, wrapping, bit aliases, and other required semantics. Script diagnostics and traces retain the original address and symbol, unmapped access fails explicitly, and Original Save Image conversion remains a separate packed-layout adapter.
