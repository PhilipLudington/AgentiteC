# Injection Prevention

Injection vulnerabilities occur when untrusted input is passed to an interpreter without proper validation or escaping.

## Types of Injection in C

1. **Command Injection** - Shell commands
2. **Format String Injection** - printf family
3. **Path Traversal** - File system access
4. **SQL Injection** - Database queries (when using C with databases)

---

## Command Injection

### The Vulnerability

```c
// VULNERABLE - User input passed directly to shell
void list_directory(const char *path) {
    char cmd[256];
    sprintf(cmd, "ls -la %s", path);
    system(cmd);  // DANGEROUS!
}

// Attack: path = "; rm -rf /"
// Executes: ls -la ; rm -rf /
```

### Prevention Strategies

#### 1. Avoid system() When Possible

```c
// SAFE - Use direct API instead of shell
#include <dirent.h>

bool list_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
    return true;
}
```

#### 2. Use execve() Family (No Shell)

```c
// SAFER - No shell interpretation
#include <unistd.h>
#include <sys/wait.h>

bool run_ls(const char *path) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char *args[] = {"ls", "-la", (char *)path, NULL};
        execvp("ls", args);
        _exit(1);  // execvp failed
    } else if (pid > 0) {
        // Parent - wait for child
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    return false;
}
```

#### 3. Whitelist Validation

```c
// SAFE - Only allow known-safe characters
bool is_safe_filename(const char *name) {
    // Only allow alphanumeric, dash, underscore, dot
    for (const char *p = name; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.') {
            return false;
        }
    }
    // Also reject empty string and hidden files
    return name[0] != '\0' && name[0] != '.';
}

bool list_directory(const char *path) {
    if (!is_safe_filename(path)) {
        set_error("Invalid directory name");
        return false;
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ls -la '%s'", path);
    return system(cmd) == 0;
}
```

---

## Format String Injection

### The Vulnerability

```c
// VULNERABLE - User controls format string
void log_message(const char *user_input) {
    printf(user_input);  // DANGEROUS!
}

// Attack: user_input = "%s%s%s%s%s%s%s%s%s%s"
// Reads from stack, crashes or leaks data

// Attack: user_input = "%n%n%n%n"
// Writes to memory, potential code execution!
```

### Prevention

```c
// SAFE - Always use format specifier
void log_message(const char *user_input) {
    printf("%s", user_input);  // Safe
}

// SAFE - User input is never a format string
void log_formatted(const char *fmt, ...) {
    // fmt should be a literal, never user input
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

// Usage - format is hardcoded
log_formatted("User said: %s\n", user_input);  // Safe
```

**Rule: Never pass user input as the format string argument.**

---

## Path Traversal

### The Vulnerability

```c
// VULNERABLE - User can escape directory
char *load_user_file(const char *username, const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "/data/users/%s/%s", username, filename);
    return read_file(path);
}

// Attack: filename = "../../../etc/passwd"
// Reads: /data/users/john/../../../etc/passwd = /etc/passwd
```

### Prevention Strategies

#### 1. Validate Path Components

```c
// SAFE - Reject dangerous path components
bool is_safe_path_component(const char *name) {
    // Reject empty, current dir, parent dir
    if (!name || !*name) return false;
    if (strcmp(name, ".") == 0) return false;
    if (strcmp(name, "..") == 0) return false;

    // Reject path separators
    if (strchr(name, '/') || strchr(name, '\\')) return false;

    // Reject null bytes
    if (strchr(name, '\0') != name + strlen(name)) return false;

    return true;
}
```

#### 2. Canonicalize and Verify

```c
#include <stdlib.h>
#include <string.h>

// SAFE - Resolve path and verify it's within allowed directory
char *safe_path_join(const char *base_dir, const char *user_path) {
    char temp_path[PATH_MAX];
    char resolved_base[PATH_MAX];
    char resolved_full[PATH_MAX];

    // Resolve base directory
    if (!realpath(base_dir, resolved_base)) {
        set_error("Invalid base directory");
        return NULL;
    }

    // Build full path
    snprintf(temp_path, sizeof(temp_path), "%s/%s", base_dir, user_path);

    // Resolve full path
    if (!realpath(temp_path, resolved_full)) {
        set_error("Invalid path");
        return NULL;
    }

    // Verify resolved path is within base
    size_t base_len = strlen(resolved_base);
    if (strncmp(resolved_full, resolved_base, base_len) != 0 ||
        (resolved_full[base_len] != '/' && resolved_full[base_len] != '\0')) {
        set_error("Path traversal detected");
        return NULL;
    }

    return strdup(resolved_full);
}
```

#### 3. Use Chroot or Sandboxing

For high-security applications, isolate file access to a chroot jail or use OS-level sandboxing.

---

## SQL Injection (When Using Databases)

### The Vulnerability

```c
// VULNERABLE - String concatenation with user input
void find_user(sqlite3 *db, const char *username) {
    char query[256];
    sprintf(query, "SELECT * FROM users WHERE name = '%s'", username);
    sqlite3_exec(db, query, callback, NULL, NULL);  // DANGEROUS!
}

// Attack: username = "'; DROP TABLE users; --"
```

### Prevention

```c
// SAFE - Use parameterized queries
bool find_user(sqlite3 *db, const char *username, User *out_user) {
    const char *query = "SELECT * FROM users WHERE name = ?";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    // Bind user input as parameter (properly escaped)
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Read user data...
        found = true;
    }

    sqlite3_finalize(stmt);
    return found;
}
```

**Rule: Always use parameterized queries, never string concatenation.**

---

## General Prevention Principles

### 1. Input Validation

```c
typedef enum {
    VALIDATE_OK,
    VALIDATE_TOO_LONG,
    VALIDATE_INVALID_CHAR,
    VALIDATE_EMPTY,
} ValidationResult;

ValidationResult validate_username(const char *username) {
    if (!username || !*username) {
        return VALIDATE_EMPTY;
    }

    size_t len = 0;
    for (const char *p = username; *p; p++) {
        len++;
        if (len > MAX_USERNAME_LENGTH) {
            return VALIDATE_TOO_LONG;
        }
        // Allow only alphanumeric and underscore
        if (!isalnum(*p) && *p != '_') {
            return VALIDATE_INVALID_CHAR;
        }
    }

    return VALIDATE_OK;
}
```

### 2. Output Encoding

When embedding user data in output, encode appropriately:

```c
// For HTML output
void html_encode(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < output_size - 6; i++) {
        switch (input[i]) {
            case '<':  j += snprintf(output + j, output_size - j, "&lt;"); break;
            case '>':  j += snprintf(output + j, output_size - j, "&gt;"); break;
            case '&':  j += snprintf(output + j, output_size - j, "&amp;"); break;
            case '"':  j += snprintf(output + j, output_size - j, "&quot;"); break;
            case '\'': j += snprintf(output + j, output_size - j, "&#39;"); break;
            default:   output[j++] = input[i]; break;
        }
    }
    output[j] = '\0';
}
```

### 3. Principle of Least Privilege

- Run with minimum necessary permissions
- Drop privileges after initialization
- Use separate processes for dangerous operations

---

## Checklist

Before submitting code:

- [ ] No `system()` calls with user input
- [ ] No user input as format strings
- [ ] All file paths validated against traversal
- [ ] Database queries use parameterized statements
- [ ] Input validated using whitelist approach
- [ ] Output properly encoded for context
- [ ] Privileges minimized where possible
