Import("env")

import subprocess


def git_hash():
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short=8", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        if out:
            return out
    except Exception:
        pass
    return "unknown"


env.Append(CPPDEFINES=[("GIT_HASH", env.StringifyMacro(git_hash()))])
