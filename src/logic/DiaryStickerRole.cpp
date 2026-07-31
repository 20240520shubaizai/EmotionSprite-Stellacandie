#include "DiaryStickerRole.h"

namespace {
struct Choice { QString emoji; QString label; };
void add(QList<Choice> &out, const QString &emoji, const QString &label)
{
    for (const auto &item : out) if (item.emoji == emoji) return;
    out.append({emoji, label});
}
}

QList<DiaryStickerRecord> DiaryStickerRole::generate(const QDate &date, const PetStateRecord &state,
                                                      const DreamRecord *dream)
{
    QList<Choice> choices;
    if (state.mood >= 72) add(choices, QStringLiteral("🌈"), QStringLiteral("好心情"));
    if (state.mood <= 38) add(choices, QStringLiteral("☁️"), QStringLiteral("慢慢来"));
    if (state.energy <= 38) add(choices, QStringLiteral("💤"), QStringLiteral("困困"));
    if (state.health <= 65) add(choices, QStringLiteral("🩹"), QStringLiteral("要休息"));
    if (state.closeness >= 70) add(choices, QStringLiteral("🧶"), QStringLiteral("贴贴"));

    if (dream) {
        const QString text = dream->title + dream->content + dream->symbols.join(QLatin1Char(' '));
        const QList<QPair<QString, Choice>> map{
            {QStringLiteral("猫"), {QStringLiteral("🐾"), QStringLiteral("猫爪印")}},
            {QStringLiteral("蛋糕"), {QStringLiteral("🍰"), QStringLiteral("甜甜的梦")}},
            {QStringLiteral("月"), {QStringLiteral("🌙"), QStringLiteral("月光")}},
            {QStringLiteral("星"), {QStringLiteral("⭐"), QStringLiteral("亮晶晶")}},
            {QStringLiteral("书"), {QStringLiteral("📖"), QStringLiteral("故事页")}},
            {QStringLiteral("雨"), {QStringLiteral("☔"), QStringLiteral("雨滴")}},
            {QStringLiteral("花"), {QStringLiteral("🌼"), QStringLiteral("小花")}}
        };
        for (const auto &item : map) if (text.contains(item.first)) add(choices, item.second.emoji, item.second.label);
    }

    const QList<Choice> fallback{{QStringLiteral("✨"), QStringLiteral("闪闪发光")},
        {QStringLiteral("🍬"), QStringLiteral("一点甜")}, {QStringLiteral("🍃"), QStringLiteral("轻轻吹过")},
        {QStringLiteral("☕"), QStringLiteral("歇一会儿")}, {QStringLiteral("🌼"), QStringLiteral("今日小花")},
        {QStringLiteral("🐾"), QStringLiteral("路过一下")}};
    const uint seed = qHash(date.toString(Qt::ISODate));
    for (int i = 0; choices.size() < 4 && i < fallback.size(); ++i) {
        const auto &choice = fallback.at((seed + uint(i * 3)) % uint(fallback.size()));
        add(choices, choice.emoji, choice.label);
    }

    const int xs[] = {7, 76, 8, 77}, ys[] = {10, 18, 72, 78}, rotations[] = {-8, 7, 5, -6};
    QList<DiaryStickerRecord> result;
    const int count = 1 + int(seed % 4); // 严格少于五枚
    for (int i = 0; i < count && i < choices.size(); ++i) {
        const auto &choice = choices.at(int((seed + uint(i * 5)) % uint(choices.size())));
        DiaryStickerRecord sticker;
        sticker.entryDate = date; sticker.emoji = choice.emoji; sticker.label = choice.label;
        sticker.xPercent = xs[i]; sticker.yPercent = ys[i]; sticker.rotation = rotations[i];
        result.append(sticker);
    }
    return result;
}
