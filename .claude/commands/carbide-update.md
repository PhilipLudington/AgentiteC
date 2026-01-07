# Carbide Update

Update the Carbide framework to the latest version.

## Instructions

1. **Remove the old Carbide directory**:
   ```bash
   rm -rf carbide
   ```

2. **Clone the latest version**:
   ```bash
   git clone https://github.com/PhilipLudington/Carbide.git carbide
   rm -rf carbide/.git
   ```

3. **Update Claude Code integration**:
   ```bash
   cp carbide/commands/*.md .claude/commands/
   cp carbide/rules/*.md .claude/rules/
   ```

4. **Update tooling configs** (optional):
   ```bash
   cp carbide/templates/.clang-format .
   cp carbide/templates/.clang-tidy .
   ```

5. **Verify update**:
   - Confirm `.claude/commands/` and `.claude/rules/` are updated
