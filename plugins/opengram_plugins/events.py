"""Event payloads passed from the host (C++ later) into plugins. Skeleton only — beta."""

from dataclasses import dataclass


@dataclass
class Message:
    chat: str
    sender: str
    text: str
