# Interpret original campaign scripts in native C++

Status: Accepted

Oracle Redux preserves ROM-supplied Campaign Scripts and executes them through a rewritten Native Script Runtime. The runtime decodes script instructions into typed, source-addressed operations and preserves their control flow, waits, actor-local state, and logic-tick scheduling. It never executes LR35902 machine code or arbitrary ROM addresses: script operations that originally call assembly helpers resolve to an explicit registry of named C++ host commands.
