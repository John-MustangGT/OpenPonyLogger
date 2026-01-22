#!/usr/bin/env python3
"""
Generate version_info.h with git information at build time.
This script is called by PlatformIO before compilation.
"""
Import("env")
import subprocess
import os
from datetime import datetime

def get_git_info():
    """Extract git SHA, branch, and tag information."""
    git_sha = "unknown"
    git_branch = "unknown"
    git_tag = "unknown"
    git_dirty = False

    try:
        # Get current commit SHA (short form)
        result = subprocess.run(
            ["git", "rev-parse", "--short=7", "HEAD"],
            capture_output=True,
            text=True,
            cwd=env.get("PROJECT_DIR")
        )
        if result.returncode == 0:
            git_sha = result.stdout.strip()

        # Check if working tree is dirty
        result = subprocess.run(
            ["git", "diff", "--quiet"],
            cwd=env.get("PROJECT_DIR")
        )
        git_dirty = (result.returncode != 0)

        # Get current branch
        result = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True,
            text=True,
            cwd=env.get("PROJECT_DIR")
        )
        if result.returncode == 0:
            git_branch = result.stdout.strip()

        # Get latest tag (if any)
        result = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            capture_output=True,
            text=True,
            cwd=env.get("PROJECT_DIR")
        )
        if result.returncode == 0:
            git_tag = result.stdout.strip()
        else:
            # No tags yet, use "v0.0.0"
            git_tag = "v0.0.0"

    except Exception as e:
        print(f"Warning: Could not get git info: {e}")

    return git_sha, git_branch, git_tag, git_dirty

def generate_version_header():
    """Generate version_info.h with git information."""
    git_sha, git_branch, git_tag, git_dirty = get_git_info()

    # Add dirty flag to SHA if needed
    if git_dirty:
        git_sha += "-dirty"

    # Get build timestamp
    build_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # Generate header content
    header_content = f'''#ifndef VERSION_INFO_H
#define VERSION_INFO_H

// Auto-generated at build time by scripts/generate_version.py
// DO NOT EDIT MANUALLY - changes will be overwritten

#define PROJECT_NAME "OpenPonyLogger"
#define PROJECT_LICENSE "MIT"

// Git information (captured at build time)
#define GIT_COMMIT_SHA "{git_sha}"
#define GIT_BRANCH "{git_branch}"
#define GIT_TAG "{git_tag}"

// Build timestamp
#define BUILD_TIMESTAMP "{build_time}"

// Version string formats
inline const char* get_version_string() {{
    static char version_buffer[128];
    snprintf(version_buffer, sizeof(version_buffer),
             "%s %s (%.7s on %s)",
             PROJECT_NAME, GIT_TAG, GIT_COMMIT_SHA, GIT_BRANCH);
    return version_buffer;
}}

inline const char* get_full_version_string() {{
    static char version_buffer[256];
    snprintf(version_buffer, sizeof(version_buffer),
             "%s %s\\nCommit: %s\\nBranch: %s\\nBuilt: %s",
             PROJECT_NAME, GIT_TAG, GIT_COMMIT_SHA, GIT_BRANCH, BUILD_TIMESTAMP);
    return version_buffer;
}}

#endif // VERSION_INFO_H
'''

    # Write to include directory
    output_path = os.path.join(env.get("PROJECT_DIR"), "lib", "Config", "include", "version_info.h")

    # Only write if content changed (avoid unnecessary rebuilds)
    write_file = True
    if os.path.exists(output_path):
        with open(output_path, 'r') as f:
            existing_content = f.read()
        if existing_content == header_content:
            write_file = False

    if write_file:
        with open(output_path, 'w') as f:
            f.write(header_content)
        print(f"Generated version_info.h: {git_tag} ({git_sha} on {git_branch})")
    else:
        print(f"version_info.h unchanged: {git_tag} ({git_sha} on {git_branch})")

# Run the generation
generate_version_header()
