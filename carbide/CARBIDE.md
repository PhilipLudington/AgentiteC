# Carbide - AI Development Guide

This document provides a quick reference for Claude Code when working on Carbide C/C++ projects.

## Rules

Detailed coding rules are automatically loaded from `.claude/rules/`:

| Rule File | Coverage |
|-----------|----------|
| `memory.md` | Ownership, allocation, NULL safety |
| `errors.md` | Return values, error checking, cleanup patterns |
| `security.md` | Input validation, buffer safety, injection prevention |
| `naming.md` | Case conventions, prefixes, function patterns |
| `api-design.md` | Config structs, const correctness, headers |
| `testing.md` | Test organization, naming, sanitizers |
| `concurrency.md` | Thread safety, mutexes, atomics |
| `preprocessor.md` | Macro hygiene, conditional compilation |
| `logging.md` | Log levels, message format, security |
| `portability.md` | Integer types, byte order, platform abstraction |

## Slash Commands

| Command | Purpose |
|---------|---------|
| `/carbide-install` | Install Carbide into an existing project |
| `/carbide-update` | Update Carbide to the latest version |
| `/carbide-review` | Review code against Carbide standards |
| `/carbide-safety` | Security-focused review (CWE/OWASP) |
| `/carbide-check` | Run validation tooling (build, lint, test) |
| `/carbide-init` | Scaffold a new Carbide project |

## Documentation

| Document | Purpose |
|----------|---------|
| `STANDARDS.md` | Complete coding standards with rationale and examples |
| `docs/patterns/` | Implementation patterns (memory, errors, API, resources) |
| `docs/security/` | Security guides (buffer overflow, memory safety, injection) |

## Core Principles

1. **Explicit ownership** - Every pointer has exactly one owner
2. **Fail fast** - Check errors immediately, propagate clearly
3. **Validate at boundaries** - Trust internal code, validate external input
4. **Simple over clever** - Readable code beats optimizations

## Quick Checklist

Before completing any C/C++ code:

- [ ] All allocations checked for NULL
- [ ] All allocations have matching deallocations
- [ ] Error paths clean up resources
- [ ] External input validated before use
- [ ] Bounded string functions used (snprintf, not sprintf)
- [ ] Array indices validated before access
- [ ] Public functions documented
- [ ] Thread safety documented (if applicable)
