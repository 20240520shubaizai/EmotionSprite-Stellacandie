from enum import StrEnum


class AgentExecutionError(StrEnum):
    model_unavailable="model_unavailable"
    model_timeout="model_timeout"
    empty_body="empty_body"
    format_error="format_error"
    validation_failed="validation_failed"
