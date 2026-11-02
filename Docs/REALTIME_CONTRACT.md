# Real-time Audio Contract

Inside any audio callback / process() method the following are forbidden:

- Heap allocation / free
- Mutex / lock acquisition
- File or network I/O
- Logging / console output
- Unbounded queues
- Blocking system calls

All buffers and coefficient updates must be prepared outside the callback or performed with lock-free atomics / double-buffering.

Denormals are mitigated by the caller (ScopedNoDenormals) or by explicit flushing if needed.

Sample-rate and block-size changes are handled by explicit `setSampleRate` + `reset` calls before the next process cycle.
