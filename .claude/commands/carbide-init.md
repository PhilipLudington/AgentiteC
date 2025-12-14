# Carbide Project Initialization

Create a new Carbide-compliant C project with full scaffolding.

## Arguments
- `$ARGUMENTS` - Project name (required)

## Instructions

Create a new C project following Carbide standards. The project should be created in a directory named after the project argument.

### Directory Structure to Create

```
{project_name}/
├── include/{project_name}/
│   └── {project_name}.h      # Main public header
├── src/
│   ├── main.c                # Entry point
│   └── {project_name}.c      # Implementation
├── tests/
│   └── test_main.c           # Test file
├── Makefile                  # Build system
├── .clang-tidy               # Static analysis config
├── .clang-format             # Formatting config
├── README.md                 # Project readme
└── .gitignore                # Git ignore file
```

### Files to Generate

#### 1. Main Header (`include/{project_name}/{project_name}.h`)

Generate a header following CARBIDE.md "Header File Template":
- Include guards using `{PROJECT_NAME}_H` format
- `extern "C"` guards for C++ compatibility
- Section comments for Types, Lifecycle, Operations
- A sample opaque type `{ProjectName}_Context`
- Config struct with DEFAULT macro
- Create/destroy functions

#### 2. Implementation (`src/{project_name}.c`)

Generate implementation following CARBIDE.md "Source File Template":
- Struct definition for the opaque type
- Thread-local error handling (`set_error`, `get_last_error`)
- Create function with NULL config handling, allocation check
- Destroy function with NULL safety

#### 3. Main Entry Point (`src/main.c`)

Simple main that:
- Creates context with default config
- Checks for errors
- Prints success message
- Cleans up and returns

#### 4. Test File (`tests/test_main.c`)

Basic tests:
- Test create/destroy
- Test NULL config handling
- Test error on invalid input

#### 5. Makefile

Use the Carbide Makefile template from `templates/Makefile` with:
- Project name substituted
- All standard targets (build, check, safety, test, format, clean)
- Cross-platform support (detect OS, compiler)

#### 6. .clang-tidy

Copy from `templates/.clang-tidy`

#### 7. .clang-format

Copy from `templates/.clang-format`

#### 8. README.md

Generate project-specific README with:
- Project name and description placeholder
- Build instructions
- Usage example

#### 9. .gitignore

Standard C gitignore:
```
# Build artifacts
build/
*.o
*.a
*.so
*.dylib

# Executables
{project_name}
{project_name}.exe

# IDE
.vscode/
.idea/
*.swp

# OS
.DS_Store
Thumbs.db
```

### Naming Convention

Convert project name to appropriate cases:
- Directory/files: `{project_name}` (snake_case)
- Type prefix: `{ProjectName}_` (PascalCase)
- Function prefix: `{project_name}_` (snake_case)
- Header guard: `{PROJECT_NAME}_H` (UPPER_SNAKE_CASE)

### After Creation

1. Report what was created
2. Show how to build: `cd {project_name} && make`
3. Show how to run: `./build/{project_name}`
4. Show how to check: `make check`
