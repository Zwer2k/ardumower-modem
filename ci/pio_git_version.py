
"""
PlatformIO pre-build script to inject Git version information
as compiler defines - equivalent to ci/bin/git-tag.sh used in Taskfile.
"""

import subprocess
import os

Import("env")  # noqa: F821 (PlatformIO injects this)

print("[git_version] PROJECT_DIR = " + env.subst("$PROJECT_DIR"))
print("[git_version] Current dir = " + os.getcwd())


def _run(cmd):
    print("[git_version] Running: " + ' '.join(cmd))
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=env.subst("$PROJECT_DIR"),
    )
    if result.returncode != 0:
        print("[git_version] Command failed with return code " + str(result.returncode))
        print("[git_version] stderr: " + result.stderr)
    print("[git_version] Output: " + result.stdout)
    return result.stdout.strip()


def get_git_hash():
    return _run(["git", "log", "-1", "--format=%h"])


def get_git_time():
    return _run(["git", "log", "-1", "--format=%cI"])


def get_git_tag():
    """Return the tag of the current commit or an empty string."""
    refs = _run(["git", "log", "-1", "--format=%D"])
    for token in refs.replace(", ", ",").split(","):
        parts = token.strip().split(" ", 1)
        if len(parts) == 2 and parts[0] == "tag:":
            return parts[1]
    return ""


git_hash = get_git_hash()
git_time = get_git_time()
git_tag  = get_git_tag()


def make_tag(value):
    # Produces: \" so that C sees a string literal
    return '\\"' + value + '\\"'


if git_hash:
    print("[git_version] GIT_HASH = " + git_hash)
    env.Append(CPPDEFINES=[("GIT_HASH", make_tag(git_hash))])
else:
    print("[git_version] GIT_HASH = (no hash)")

if git_time:
    print("[git_version] GIT_TIME = " + git_time)
    env.Append(CPPDEFINES=[("GIT_TIME", make_tag(git_time))])
else:
    print("[git_version] GIT_TIME = (no time)")

if git_tag:
    print("[git_version] GIT_TAG  = " + git_tag)
    env.Append(CPPDEFINES=[("GIT_TAG", make_tag(git_tag))])
else:
    print("[git_version] GIT_TAG  = (no tag on this commit)")
