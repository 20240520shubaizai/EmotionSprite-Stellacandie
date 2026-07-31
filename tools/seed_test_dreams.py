"""Insert or refresh four deterministic, unopened dream papers for UI testing."""

import os
import sqlite3
import uuid
from datetime import datetime


database = os.path.join(
    os.environ["APPDATA"], "EmotionSprite", "Stellacandie", "emotion_sprite.db"
)

dreams = [
    ("2026-07-23", "云朵邮差", "我坐在一朵慢吞吞的云上，把一封没有署名的信送到你的窗边。信封打开后没有字，只有一小片暖暖的晨光。", "warm", "random_fantasy", "云朵\n信封\n晨光", "#E8D7F3", "也许今天会收到一句意外的好消息。", "cloud-letter"),
    ("2026-07-22", "抹茶月亮", "月亮变成了一块软绵绵的抹茶蛋糕，我偷偷切下一角，想留到醒来以后和你一起尝尝。", "sweet", "wish_dream", "抹茶蛋糕\n月亮\n银色叉子", "#DCEACF", "现实里若出现绿色甜点，也许就是梦留下的碎屑。", "matcha-moon"),
    ("2026-07-21", "雨夜小书店", "雨点敲着玻璃，我戴着圆眼镜守在一家只在夜里出现的小书店。一本书自己翻到某一页，上面画着你还没讲完的故事。", "curious", "memory_fusion", "眼镜\n旧书\n夜雨", "#CAD8E8", "今天遇到的某句话，也许会替这个故事写下下一行。", "night-bookshop"),
    ("2026-07-20", "追着星光的橘猫", "一只胖橘猫叼走了掉在草地上的星光，我追着它跑了很久。最后它把星光放进空碗里，像是在等谁来分享晚餐。", "playful", "random_fantasy", "橘猫\n草地\n星光小碗", "#F4D4B5", "若今天看见猫、草地或圆圆的小碗，梦可能会轻轻回应。", "orange-cat-star"),
]

with sqlite3.connect(database) as connection:
    for date, title, content, mood, kind, symbols, color, hint, key in dreams:
        connection.execute(
            """INSERT INTO dreams(
                uuid,dream_date,title,content,mood,dream_type,symbols,color,
                reality_hint,continuation_key,memory_ids,created_at,opened_at,
                favorite,reality_echo,echo_created_at,deleted_at,sync_status
            ) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,NULL,0,NULL,NULL,NULL,'pending')
            ON CONFLICT(dream_date) DO UPDATE SET
                title=excluded.title, content=excluded.content, mood=excluded.mood,
                dream_type=excluded.dream_type, symbols=excluded.symbols,
                color=excluded.color, reality_hint=excluded.reality_hint,
                continuation_key=excluded.continuation_key, opened_at=NULL,
                favorite=0, reality_echo=NULL, echo_created_at=NULL,
                deleted_at=NULL, sync_status='pending'""",
            (
                uuid.uuid4().hex, date, title, content, mood, kind, symbols,
                color, hint, key, "", datetime.now().isoformat(timespec="milliseconds"),
            ),
        )

print("Seeded 4 unopened dream papers.")
