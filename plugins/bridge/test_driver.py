"""Stand-in for the C++ host: spawn the sidecar, send events, read actions.

Proves the bridge end-to-end without compiling C++. Skeleton only — beta.
"""

import json
import subprocess
import sys
from pathlib import Path

PLUGINS_ROOT = Path(__file__).resolve().parent.parent
EXAMPLES = PLUGINS_ROOT / "examples"


def main():
    proc = subprocess.Popen(
        [sys.executable, "-m", "opengram_plugins.sidecar", str(EXAMPLES)],
        cwd=str(PLUGINS_ROOT),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    def send(event):
        proc.stdin.write(json.dumps(event) + "\n")
        proc.stdin.flush()

    for _ in range(4):
        send({"event": "message", "chat": "general", "sender": "bob", "text": "ping"})
    send({"event": "message", "chat": "general", "sender": "bob", "text": "ping"})
    send({"event": "reload"})
    send({"event": "message", "chat": "general", "sender": "bob", "text": "ping"})
    send({"event": "shutdown"})
    proc.stdin.close()

    for line in proc.stdout:
        print("C++ <-", line.strip())
    proc.wait()


if __name__ == "__main__":
    main()
