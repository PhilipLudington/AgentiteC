#!/bin/bash

# Enable debug logging if STATUSLINE_DEBUG is set
DEBUG=${STATUSLINE_DEBUG:-0}

debug_log() {
    if [ "$DEBUG" = "1" ]; then
        echo "[statusline-debug] $*" >&2
    fi
}

# Read input JSON from stdin
input=$(cat)

# Extract values from JSON
cwd=$(echo "$input" | jq -r '.workspace.current_directory' 2>/dev/null)
project_dir=$(echo "$input" | jq -r '.workspace.project_directory' 2>/dev/null)
style=$(echo "$input" | jq -r '.output_style.name' 2>/dev/null)

if [ -z "$cwd" ] || [ "$cwd" = "null" ]; then
    debug_log "Failed to parse cwd from input JSON"
    cwd="."
fi

# Time
time=$(date +%H:%M)

# Directory name - show as ProjectName/relative/path if in subdirectory
dir=''
project_name="Carbon"
if [ "$cwd" != "$project_dir" ] && [ "$project_dir" != 'null' ]; then
    rel_path=$(python3 -c "import os.path; print(os.path.relpath('$cwd', '$project_dir'))" 2>/dev/null)
    if [ $? -ne 0 ]; then
        debug_log "Failed to calculate relative path from '$cwd' to '$project_dir'"
        dir=$(basename "$cwd")
    elif [ "$rel_path" != '.' ]; then
        dir="${project_name}/${rel_path}"
    else
        # At project root
        dir="$project_name"
    fi
else
    # No project dir or at project root
    dir="$project_name"
fi

# Git branch and status
branch=''
status=''
git_changes=''
if [ -d "$cwd/.git" ] || git -C "$cwd" rev-parse --git-dir >/dev/null 2>&1; then
    branch=$(cd "$cwd" 2>/dev/null && git --no-optional-locks branch --show-current 2>/dev/null || echo '')
    if [ -z "$branch" ]; then
        debug_log "Git repository detected but failed to get current branch"
    fi
    if [ -n "$branch" ]; then
        # Count changes
        modified=$(git -C "$cwd" --no-optional-locks diff --name-only 2>/dev/null | wc -l | tr -d ' ')
        staged=$(git -C "$cwd" --no-optional-locks diff --cached --name-only 2>/dev/null | wc -l | tr -d ' ')
        untracked=$(git -C "$cwd" --no-optional-locks ls-files --others --exclude-standard 2>/dev/null | wc -l | tr -d ' ')

        if [ "$modified" != '0' ] || [ "$staged" != '0' ] || [ "$untracked" != '0' ]; then
            status='●'
            changes=''
            [ "$modified" != '0' ] && changes="${changes}\033[33m${modified}M\033[0m "
            [ "$staged" != '0' ] && changes="${changes}\033[32m${staged}S\033[0m "
            [ "$untracked" != '0' ] && changes="${changes}\033[31m${untracked}U\033[0m"
            changes=$(echo "$changes" | sed 's/ $//')
            [ -n "$changes" ] && git_changes=" ${changes}"
        else
            status='✓'
        fi
    fi
fi

# C/Make build status indicator
# Check if last build succeeded by looking for the executable
build_status=''
if [ "$project_dir" != 'null' ] && [ -n "$project_dir" ]; then
    if [ -f "${project_dir}/build/carbon" ]; then
        build_status=" \033[2m[\033[0m\033[32mBuild ✓\033[0m\033[2m]\033[0m"
    elif [ -d "${project_dir}/build" ] && [ "$(ls -A ${project_dir}/build 2>/dev/null)" ]; then
        # Has build dir with files but no executable - might be building or failed
        build_status=" \033[2m[\033[0m\033[33mBuild ~\033[0m\033[2m]\033[0m"
    fi
fi

# Test health indicator
# Reads from .test-results file (updated by test runs)
test_status=''
if [ "$project_dir" != 'null' ] && [ -n "$project_dir" ]; then
    test_results_file="${project_dir}/.test-results"
    if [ -f "$test_results_file" ]; then
        test_result=$(cat "$test_results_file" 2>/dev/null)
        if [ "$test_result" = "pass" ]; then
            test_status=" \033[2m[\033[0m\033[32mTests ✓\033[0m\033[2m]\033[0m"
        elif [ "$test_result" = "fail" ]; then
            test_status=" \033[2m[\033[0m\033[31mTests ✗\033[0m\033[2m]\033[0m"
        fi
    fi
fi

# Build output string
out="\033[2m${time}\033[0m \033[36m${dir}\033[0m"

# Add git branch and status
if [ -n "$branch" ]; then
    if [ "$status" = '●' ]; then
        out="${out} \033[2m[\033[0m\033[33m${branch}\033[0m \033[31m${status}\033[0m${git_changes}\033[2m]\033[0m"
    else
        out="${out} \033[2m[\033[0m\033[32m${branch}\033[0m \033[32m${status}\033[0m${git_changes}\033[2m]\033[0m"
    fi
fi

# Add build status
[ -n "$build_status" ] && out="${out}${build_status}"

# Add test status
[ -n "$test_status" ] && out="${out}${test_status}"

# Add output style if not default
if [ "$style" != 'default' ] && [ "$style" != 'null' ]; then
    out="${out} \033[2m(\033[0m\033[35m${style}\033[0m\033[2m)\033[0m"
fi

# Print the status line
printf "%b" "$out"
