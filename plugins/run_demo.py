"""Tiny beta runner: load .plg plugins, feed fake events, unload. Skeleton only.

Later the C++ host replaces FakeHost and feeds real messages into dispatch_message.
"""

from pathlib import Path

from opengram_plugins import Message, PluginManager

if __name__ == "__main__":
    manager = PluginManager(Path(__file__).parent / "examples")
    manager.load_all()
    print("loaded:", [p.name for p in manager.plugins])

    manager.dispatch_message(Message(chat="general", sender="alice", text="hi"))
    manager.dispatch_message(Message(chat="general", sender="bob", text="ping"))

    manager.unload_all()
