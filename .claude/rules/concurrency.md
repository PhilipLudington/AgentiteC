---
paths: "**/*.{c,h,cpp,hpp}"
---

# Concurrency Rules

## Thread Safety Documentation

- Document thread safety of every public function
- Use comments: `/* Thread-safe */` or `/* NOT thread-safe */`
- Document which locks must be held by caller, if any

## Mutex Usage

- **C1**: Always use RAII-style lock/unlock patterns
- **C2**: Release locks in reverse order of acquisition (prevent deadlock)
- **C3**: Never hold locks while calling external/callback functions
- **C4**: Keep critical sections as short as possible

```c
void safe_operation(Context *ctx) {
    mtx_lock(&ctx->mutex);
    // Critical section - keep minimal
    int value = ctx->shared_value;
    mtx_unlock(&ctx->mutex);

    // Do work outside lock
    process(value);
}
```

## Atomic Operations

- Use `_Atomic` types for simple shared counters/flags
- Prefer atomics over mutexes for single-variable synchronization
- Use appropriate memory ordering (default to `memory_order_seq_cst` if unsure)

## Thread Creation

- Always check thread creation return values
- Ensure threads are joined or detached before program exit
- Pass thread arguments by pointer to heap-allocated data, not stack

## Avoiding Data Races

- Never access shared data without synchronization
- Use `_Thread_local` for thread-specific data
- Initialize shared data before spawning threads

## Condition Variables

- Always check condition in a loop (spurious wakeups)
- Signal/broadcast while holding the associated mutex
- Prefer `cnd_signal` over `cnd_broadcast` when waking one thread

## Deadlock Prevention

- Establish a global lock ordering and document it
- Use try-lock with timeout for lock acquisition when possible
- Avoid nested locks when possible

## Reentrant Functions

- A reentrant function must not use static/global data
- A reentrant function must not call non-reentrant functions
- Use `_r` suffix for reentrant variants: `strtok_r`
