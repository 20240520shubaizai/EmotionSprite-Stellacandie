import logging


class MetadataOnlyFilter(logging.Filter):
    blocked = ("payload", "message_text", "secret", "api_key", "authorization")

    def filter(self, record: logging.LogRecord) -> bool:
        text = record.getMessage().lower()
        return not any(word in text for word in self.blocked)


def configure_logging() -> None:
    handler = logging.StreamHandler()
    handler.addFilter(MetadataOnlyFilter())
    handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s %(message)s"))
    root = logging.getLogger()
    root.handlers[:] = [handler]
    root.setLevel(logging.INFO)
