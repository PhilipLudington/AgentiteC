---
paths: "**/*.{c,h,cpp,hpp}"
---

# Naming Convention Rules

## Case Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Types (struct, enum, typedef) | `PascalCase` | `PlayerState`, `TextureFormat` |
| Functions | `snake_case` | `player_create`, `texture_load` |
| Variables (local, parameters) | `snake_case` | `player_count`, `is_valid` |
| Struct/union fields | `snake_case` | `health`, `max_speed` |
| Macros and constants | `UPPER_SNAKE_CASE` | `MAX_PLAYERS`, `PI` |
| Enum values | `UPPER_SNAKE_CASE` | `STATE_IDLE`, `STATE_RUNNING` |

## Prefixing

- All public symbols MUST have a project prefix (e.g., `carbide_`, `mylib_`)
- Private/static symbols do not require prefix
- Type prefixes match function prefixes: `MyLib_Player` with `mylib_player_create`

## Function Naming Patterns

| Suffix | Meaning |
|--------|---------|
| `_create` / `_destroy` | Allocate/deallocate (ownership transfer) |
| `_init` / `_deinit` | Initialize/cleanup caller-owned memory |
| `_load` / `_unload` | Load from external source |
| `_get_` / `_set_` | Accessor/mutator |
| `_is_` / `_has_` / `_can_` | Boolean query |
| `_begin` / `_end` | Scoped operation pair |
| `_add` / `_remove` | Collection modification |
| `_find` / `_get` | Lookup (find may fail, get assumes valid) |

## Boolean Naming

- Boolean variables and functions MUST read as true/false statements
- Use prefixes: `is_`, `has_`, `can_`, `should_`, `was_`
- GOOD: `is_valid`, `has_children`, `can_attack`
- BAD: `valid`, `children`, `attack`

## Static/Global Prefixes

- Global variables: `g_` prefix (e.g., `g_config`)
- Static file-scope variables: `s_` prefix (e.g., `s_instance_count`)
- Thread-local: no special prefix, but document clearly
