# Further improvements

## Explicitly pin main thread to core 4

Currently `taskset -c 4,6` restricts the process to cores 4 and 6, and
`pthread_setaffinity_np` hard-pins the logger thread to core 6. The main thread
ending up on core 4 is an emergent consequence — the scheduler puts it there
because core 6 is fully occupied — not an explicit guarantee.

To be rigorous, call `pthread_setaffinity_np` on the main thread at startup to
hard-pin it to core 4, the same way the logger thread is pinned to core 6.
