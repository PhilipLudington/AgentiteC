---
paths: "**/*.{c,h,cpp,hpp}"
---

# Logging Rules

## Log Levels

Use consistent severity levels:

| Level | Use For |
|-------|---------|
| `ERROR` | Failures that prevent operation completion |
| `WARN` | Unexpected conditions that don't prevent operation |
| `INFO` | Significant events (startup, shutdown, connections) |
| `DEBUG` | Detailed information for troubleshooting |
| `TRACE` | Very detailed execution flow (high volume) |

## Log Message Format

- Include timestamp, level, and source location
- Use structured format: `[LEVEL] module: message (key=value)`
- Keep messages concise but informative

```c
LOG_ERROR("player: failed to load (path=%s, error=%s)", path, get_last_error());
LOG_INFO("server: client connected (addr=%s, id=%u)", addr, client_id);
```

## What to Log

- All errors with context (what failed, why, relevant values)
- State transitions and significant events
- External interactions (file I/O, network, IPC)
- Performance metrics at DEBUG level

## What NOT to Log

- **L1**: Never log secrets (passwords, tokens, keys)
- **L2**: Never log full file contents or large data blobs
- **L3**: Avoid logging in tight loops (performance impact)
- **L4**: Never log PII unless explicitly required and secured

## Performance

- Use lazy evaluation - don't format strings if level is disabled
- Provide compile-time level filtering for release builds
- Buffer log output to reduce I/O overhead

```c
// GOOD: Conditional evaluation
#define LOG_DEBUG(fmt, ...) \
    do { if (g_log_level >= LOG_LEVEL_DEBUG) log_write(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__); } while(0)
```

## Error Correlation

- Include request/transaction IDs in related log messages
- Log entry and exit of significant operations
- Include enough context to trace issues without source code
