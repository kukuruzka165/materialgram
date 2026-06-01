"""Plugin sidecar process. The C++ host spawns this once and talks NDJSON.

Protocol (one JSON object per line):
  C++ -> Python (stdin):   {"event": "message", "chat": "..", "sender": "..", "text": ".."}
                           {"event": "reload"}
                           {"event": "shutdown"}
  Python -> C++ (stdout):  {"action": "send_message", "chat": "..", "text": ".."}
                           {"action": "log", "text": ".."}

stdout is reserved for the protocol. Plugin print() is redirected to stderr so a
careless print can never corrupt the channel. Skeleton only — beta.
"""

import json
import os
import sys

from .events import Message
from .host import RpcHost
from .manager import PluginManager


def main(argv=None):
    argv = argv if argv is not None else sys.argv[1:]
    plugins_dir = argv[0] if argv else os.environ.get("OPENGRAM_PLUGINS_DIR", "examples")

    protocol_out = sys.stdout
    sys.stdout = sys.stderr

    manager = PluginManager(plugins_dir, host=RpcHost(protocol_out))
    manager.load_all()

    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            continue
        kind = event.get("event")
        if kind == "message":
            manager.dispatch_message(Message(
                chat=event.get("chat", ""),
                sender=event.get("sender", ""),
                text=event.get("text", ""),
            ))
        elif kind == "reload":
            manager.reload()
        elif kind == "shutdown":
            break

    manager.unload_all()


if __name__ == "__main__":
    main()
