# Buffer Overflow Prevention

Buffer overflows are one of the most dangerous vulnerabilities in C programs. This document explains how to prevent them.

## What is a Buffer Overflow?

A buffer overflow occurs when a program writes data beyond the allocated memory bounds.

```c
// Vulnerable code
char buffer[10];
strcpy(buffer, "This string is way too long for the buffer");
// Overwrites memory beyond buffer, causing undefined behavior
```

**Consequences:**
- Program crashes
- Data corruption
- **Security exploits** (code execution, privilege escalation)

---

## Common Vulnerable Patterns

### 1. Unbounded String Copy

```c
// VULNERABLE
char dest[64];
strcpy(dest, user_input);  // No length check!
```

**Fix:**
```c
// SAFE
char dest[64];
strncpy(dest, user_input, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';  // Ensure null termination

// BETTER: Use snprintf
char dest[64];
snprintf(dest, sizeof(dest), "%s", user_input);
```

### 2. Unbounded String Format

```c
// VULNERABLE
char buffer[128];
sprintf(buffer, "User: %s, Score: %d", username, score);  // Can overflow!
```

**Fix:**
```c
// SAFE
char buffer[128];
int written = snprintf(buffer, sizeof(buffer), "User: %s, Score: %d",
                       username, score);
if (written >= sizeof(buffer)) {
    // Output was truncated - handle appropriately
}
```

### 3. Unbounded String Concatenation

```c
// VULNERABLE
char path[256];
strcpy(path, base_dir);
strcat(path, "/");
strcat(path, filename);  // Each strcat can overflow!
```

**Fix:**
```c
// SAFE
char path[256];
int written = snprintf(path, sizeof(path), "%s/%s", base_dir, filename);
if (written >= sizeof(path)) {
    set_error("Path too long");
    return false;
}
```

### 4. gets() Function

```c
// VULNERABLE - NEVER USE gets()
char line[100];
gets(line);  // Deprecated, removed in C11
```

**Fix:**
```c
// SAFE
char line[100];
if (fgets(line, sizeof(line), stdin)) {
    // Remove newline if present
    line[strcspn(line, "\n")] = '\0';
}
```

### 5. scanf Without Width

```c
// VULNERABLE
char name[32];
scanf("%s", name);  // No limit on input length!
```

**Fix:**
```c
// SAFE - specify maximum width (one less than buffer for null)
char name[32];
scanf("%31s", name);
```

### 6. Array Index Without Bounds Check

```c
// VULNERABLE
int get_value(int *array, int index) {
    return array[index];  // No bounds check!
}
```

**Fix:**
```c
// SAFE
int get_value(const int *array, size_t array_size, size_t index) {
    if (index >= array_size) {
        set_error("Index %zu out of bounds (size %zu)", index, array_size);
        return -1;  // Or appropriate error value
    }
    return array[index];
}
```

### 7. Loop Without Proper Termination

```c
// VULNERABLE
void copy_until_null(char *dest, const char *src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];  // No check on dest size!
        i++;
    }
    dest[i] = '\0';
}
```

**Fix:**
```c
// SAFE
bool copy_string(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) return false;

    size_t i = 0;
    while (i < dest_size - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';

    // Return false if truncated
    return src[i] == '\0';
}
```

---

## Safe Functions Reference

| Unsafe | Safe Alternative | Notes |
|--------|------------------|-------|
| `sprintf` | `snprintf` | Always specify buffer size |
| `vsprintf` | `vsnprintf` | For variadic functions |
| `strcpy` | `strncpy` + null-terminate | Or use `snprintf` |
| `strcat` | `strncat` | Or use `snprintf` |
| `gets` | `fgets` | `gets` is removed in C11 |
| `scanf("%s")` | `scanf("%Ns")` | N = buffer size - 1 |

---

## Defensive Patterns

### Pattern 1: Always Track Buffer Size

```c
typedef struct {
    char *data;
    size_t size;      // Allocated size
    size_t length;    // Used length
} String;

bool string_append(String *s, const char *text) {
    size_t text_len = strlen(text);

    // Check if we have room
    if (s->length + text_len >= s->size) {
        // Grow buffer
        size_t new_size = (s->length + text_len + 1) * 2;
        char *new_data = realloc(s->data, new_size);
        if (!new_data) return false;
        s->data = new_data;
        s->size = new_size;
    }

    memcpy(s->data + s->length, text, text_len + 1);
    s->length += text_len;
    return true;
}
```

### Pattern 2: Use Fixed-Size Buffers with Validation

```c
#define MAX_NAME_LENGTH 64

typedef struct {
    char name[MAX_NAME_LENGTH];
} Player;

bool player_set_name(Player *p, const char *name) {
    if (!p || !name) return false;

    size_t len = strlen(name);
    if (len >= MAX_NAME_LENGTH) {
        set_error("Name too long (%zu chars, max %d)",
                  len, MAX_NAME_LENGTH - 1);
        return false;
    }

    strcpy(p->name, name);  // Safe: length already validated
    return true;
}
```

### Pattern 3: Use sizeof for Stack Buffers

```c
void process_input(const char *input) {
    char buffer[256];

    // ALWAYS use sizeof(buffer), never magic numbers
    if (snprintf(buffer, sizeof(buffer), "Processing: %s", input)
        >= sizeof(buffer)) {
        // Handle truncation
    }
}
```

### Pattern 4: Validate All External Data

```c
bool load_header(const uint8_t *data, size_t size, Header *out) {
    // Check we have enough data for header
    if (size < sizeof(Header)) {
        set_error("Data too small for header");
        return false;
    }

    memcpy(out, data, sizeof(Header));

    // Validate header fields before using them
    if (out->name_length > MAX_NAME_LENGTH) {
        set_error("Invalid name length in header: %u", out->name_length);
        return false;
    }

    if (out->data_offset > size) {
        set_error("Data offset beyond file size");
        return false;
    }

    return true;
}
```

---

## Compiler Protections

Enable these compiler features for additional protection:

### Stack Canaries
```makefile
CFLAGS += -fstack-protector-strong
```
Detects stack buffer overflows by placing "canary" values on the stack.

### FORTIFY_SOURCE
```makefile
CFLAGS += -D_FORTIFY_SOURCE=2 -O2
```
Replaces unsafe functions with checked versions at compile time.

### AddressSanitizer (Development)
```makefile
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
```
Detects buffer overflows at runtime during testing.

---

## Checklist

Before submitting code:

- [ ] No use of `sprintf`, `strcpy`, `strcat`, `gets`
- [ ] All `snprintf` calls check for truncation
- [ ] All array accesses have bounds checks
- [ ] All external input is validated before use
- [ ] Fixed-size buffers use `sizeof()`, not magic numbers
- [ ] String functions account for null terminator
- [ ] Loops have proper termination conditions
