"""Base plugin contract. Skeleton only — beta."""


class Plugin:
    name = "unnamed"
    version = "0.0.0"

    def __init__(self):
        self.host = None

    def on_load(self):
        pass

    def on_unload(self):
        pass

    def on_message(self, message):
        pass
