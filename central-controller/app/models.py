from pydantic import BaseModel, Field


class TriggerRequest(BaseModel):
    command: str = Field(min_length=1, max_length=64)


class TriggerResponse(BaseModel):
    device_id: str
    topic: str
    command: str
    published: bool
