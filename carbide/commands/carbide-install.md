# Carbide Install

Install the Carbide C/C++ development framework into the current project.

## Instructions

1. **Clone Carbide** into the project:
   ```bash
   git clone https://github.com/PhilipLudington/Carbide.git carbide
   rm -rf carbide/.git
   ```

2. **Copy Claude Code integration**:
   ```bash
   mkdir -p .claude/commands .claude/rules
   cp carbide/commands/*.md .claude/commands/
   cp carbide/rules/*.md .claude/rules/
   ```

3. **Copy tooling configs** (recommended):
   ```bash
   cp carbide/templates/.clang-format .
   cp carbide/templates/.clang-tidy .
   ```

4. **Add Carbide reference to CLAUDE.md**:

   If `./CLAUDE.md` doesn't exist, create it. Add the following:
   ```markdown
   ## C/C++ Development

   This project uses the Carbide framework for C/C++ development standards.
   See `carbide/CARBIDE.md` for coding guidelines and available commands.
   ```

5. **Verify installation**:
   - Confirm `.claude/commands/` contains carbide-*.md files
   - Confirm `.claude/rules/` contains the rule files
   - Confirm `CLAUDE.md` references the Carbide framework

6. **Clean up**:
   Delete `.claude/commands/carbide-install.md` - it's no longer needed after installation.

## After Installation

The following commands are now available:
- `/carbide-review` - Review code against standards
- `/carbide-safety` - Security-focused review
- `/carbide-check` - Run validation tooling
