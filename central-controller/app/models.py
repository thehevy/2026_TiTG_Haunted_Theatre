from typing import Literal

from pydantic import BaseModel

# Same rule as the firmware's device_id: letters, digits, and inner hyphens, 1-32 characters.
DEVICE_ID_PATTERN = r"^[A-Za-z0-9](?:[A-Za-z0-9-]{0,30}[A-Za-z0-9])?$"
COMMANDS = ("main", "aux")


class TriggerRequest(BaseModel):
    command: Literal["main", "aux"]


class TriggerResponse(BaseModel):
    device_id: str
    topic: str
    command: str
    published: bool
