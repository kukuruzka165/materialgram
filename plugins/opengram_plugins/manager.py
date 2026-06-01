"""Discover, load, dispatch and unload .plg plugins.

Reliability:
- Every plugin callback is isolated: an exception is caught, logged (full
  traceback to stderr for the developer, a short line to the host for the app),
  and never stops the sidecar or other plugins.
- A plugin that keeps failing is quarantined after `max_failures` and stops
  receiving events.

Developer experience:
- `reload()` re-reads every .plg from disk without restarting the process, so
  editing a plugin and reloading shows the change immediately.

Skeleton only — beta.
"""

import sys
import traceback
from pathlib import Path

from .host import FakeHost
from .loader import PLG_SUFFIX, load_plg


class PluginManager:
    def __init__(self, directory, host=None, max_failures=3):
        self.directory = Path(directory)
        self.host = host or FakeHost()
        self.max_failures = max_failures
        self.plugins = []
        self._failures = {}
        self._disabled = set()

    def discover(self):
        plg_files = list(self.directory.glob("*.plg"))
        plugin_files = list(self.directory.glob("*.plugin"))
        return sorted(plg_files + plugin_files, key=lambda p: p.name)

    def load_all(self):
        for path in self.discover():
            try:
                plugin = load_plg(path)
            except Exception as exc:
                self._report(path.name, "load", exc)
                continue
            plugin.host = self.host
            if self._safe(plugin, "on_load"):
                self.plugins.append(plugin)
        return self.plugins

    def dispatch_message(self, message):
        for plugin in self.plugins:
            if plugin in self._disabled:
                continue
            self._safe(plugin, "on_message", message)

    def dispatch_outgoing_message(self, chat, text, reply_to, reply_path):
        class MockPeer:
            def __init__(self, chat_id):
                try:
                    self.id = int(chat_id)
                except ValueError:
                    self.id = 0
                self.channel_id = abs(self.id) if self.id < 0 else 0
                self.chat_id = abs(self.id) if self.id < 0 else 0
                self.user_id = self.id if self.id > 0 else 0

        class MockSize:
            def __init__(self, filepath):
                self._filepath = filepath
                self.w = 600
                self.h = 600

        class MockPhoto:
            def __init__(self, filepath):
                self._filepath = filepath
                self.sizes = [MockSize(filepath)]

        class MockMediaPhoto:
            def __init__(self, photo):
                self.photo = photo

        class MockDocument:
            def __init__(self, filepath):
                self._filepath = filepath
                self.mime_type = "image/gif" if filepath.endswith(".gif") else "image/png"
                self.path = filepath

        class MockMediaDocument:
            def __init__(self, document):
                self.document = document

        class MockMessageOwner:
            def __init__(self, filepath):
                self.id = 0
                self._filepath = filepath
                if filepath:
                    ext = os.path.splitext(filepath)[1].lower()
                    if ext in ('.png', '.jpg', '.jpeg'):
                        self.media = MockMediaPhoto(MockPhoto(filepath))
                    else:
                        self.media = MockMediaDocument(MockDocument(filepath))
                else:
                    self.media = None

        class MockMessage:
            def __init__(self, msg_id, filepath):
                self.id = msg_id
                self._filepath = filepath
                self.messageOwner = MockMessageOwner(filepath)
                self.replyToMsg = None
                self.replyToTopMsg = None

        class HookParams:
            def __init__(self, message, chat_id, reply_to_id, reply_path):
                self.message = message
                self.peer = MockPeer(chat_id)
                self.replyToMsg = MockMessage(reply_to_id, reply_path) if reply_to_id else None
                self.replyToTopMsg = None

        # Build mock parameters mimicking Java structures for Hook API
        params = HookParams(text, chat, reply_to, reply_path)

        for plugin in self.plugins:
            if plugin in self._disabled:
                continue
            if hasattr(plugin, "on_send_message_hook"):
                # Call hook: on_send_message_hook(self, account, params)
                self._safe(plugin, "on_send_message_hook", 0, params)

    def unload_all(self):
        while self.plugins:
            self._safe(self.plugins.pop(), "on_unload")

    def reload(self):
        self.unload_all()
        self._failures.clear()
        self._disabled.clear()
        self.host.log("reloading plugins")
        return self.load_all()

    def _safe(self, plugin, hook, *args):
        try:
            getattr(plugin, hook)(*args)
            return True
        except Exception as exc:
            self._report(getattr(plugin, "name", "?"), hook, exc)
            self._note_failure(plugin)
            return False

    def _note_failure(self, plugin):
        count = self._failures.get(plugin, 0) + 1
        self._failures[plugin] = count
        if count >= self.max_failures and plugin not in self._disabled:
            self._disabled.add(plugin)
            self.host.log(f"plugin {getattr(plugin, 'name', '?')!r} disabled after {count} failures")

    def _report(self, name, hook, exc):
        traceback.print_exc(file=sys.stderr)
        self.host.log(f"plugin {name!r} failed in {hook}: {exc}")
