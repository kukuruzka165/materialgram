"""Load a single .plg file as a Plugin instance. Skeleton only — beta."""

from importlib.machinery import SourceFileLoader
from pathlib import Path

from .plugin import Plugin

PLG_SUFFIX = ".plg"


def load_plg(path):
    path = Path(path)
    module = SourceFileLoader(path.stem, str(path)).load_module()
    for value in vars(module).values():
        if isinstance(value, type) and issubclass(value, Plugin) and value is not Plugin:
            return value()
    raise ValueError(f"no Plugin subclass found in {path}")
