"""Host API: what a plugin can ask the app to do.

`Host` is the seam between Python and the app. Plugin authors only ever touch
`self.host.send_message(...)` / `self.host.log(...)` — never the transport.

- `FakeHost` just prints (used by run_demo, pure Python).
- `RpcHost` serialises each action to one JSON line on the protocol stream,
  which the C++ side reads. Same plugin code, different host.

Skeleton only — beta.
"""

import json


class Host:
    def send_message(self, chat, text):
        raise NotImplementedError

    def log(self, text):
        raise NotImplementedError


class FakeHost(Host):
    def __init__(self):
        self.sent = []

    def send_message(self, chat, text):
        self.sent.append((chat, text))
        print(f"  -> send_message(chat={chat!r}, text={text!r})")

    def log(self, text):
        print(f"  [host] {text}")


class RpcHost(Host):
    def __init__(self, out):
        self._out = out

    def _emit(self, action):
        self._out.write(json.dumps(action, ensure_ascii=False) + "\n")
        self._out.flush()

    def send_message(self, chat, text):
        self._emit({"action": "send_message", "chat": chat, "text": text})

    def log(self, text):
        self._emit({"action": "log", "text": text})
