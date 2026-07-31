#include "DreamGenerationRole.h"

#include <QJsonArray>
#include <QRandomGenerator>
#include <QRegularExpression>

QString DreamGenerationRole::chooseTheme()
{
    const int roll=QRandomGenerator::global()->bounded(100);
    if(roll<45)return QStringLiteral("把近期真实片段温柔地变形成梦，但明确保持梦境语气");
    if(roll<65)return QStringLiteral("从候选长期记忆中选择一条，变成带象征意味的梦");
    if(roll<85)return QStringLiteral("让精灵当前状态影响梦的节奏与色彩，不分析或教育用户");
    return QStringLiteral("生成与现实无关、童真而略带荒诞的纯幻想梦");
}

bool DreamGenerationRole::normalizeAndValidate(const QJsonObject&i,QJsonObject*out,QString*error)
{
    if(!out)return false;
    const QString title=i.value(QStringLiteral("title")).toString().trimmed().left(40);
    const QString content=i.value(QStringLiteral("content")).toString().trimmed().left(600);
    if(title.isEmpty()||content.size()<30){if(error)*error=QStringLiteral("梦境内容过短，没有形成完整的星星纸。");return false;}
    static const QRegularExpression coercive(QStringLiteral("(必须|一定要|赶紧|快去|给我买|买给我|不然我|为什么还不)"));
    if(content.contains(coercive)){if(error)*error=QStringLiteral("梦境带有催促或索取意味，已放弃本次结果。");return false;}
    const QStringList moods{QStringLiteral("warm"),QStringLiteral("funny"),QStringLiteral("quiet"),QStringLiteral("strange"),QStringLiteral("slightly_unsettled")};
    const QStringList types{QStringLiteral("memory_remix"),QStringLiteral("long_memory"),QStringLiteral("state_reflection"),QStringLiteral("random_fantasy"),QStringLiteral("fuzzy"),QStringLiteral("continuation")};
    QJsonArray symbols;for(const auto&v:i.value(QStringLiteral("symbols")).toArray()){const QString s=v.toString().trimmed().left(20);if(!s.isEmpty()&&symbols.size()<4)symbols.append(s);}if(symbols.isEmpty())symbols.append(QStringLiteral("星光"));
    QString color=i.value(QStringLiteral("color")).toString();if(!QRegularExpression(QStringLiteral("^#[0-9A-Fa-f]{6}$")).match(color).hasMatch())color=QStringLiteral("#E7C7D5");
    QJsonArray ids;for(const auto&v:i.value(QStringLiteral("memory_ids")).toArray()){const QString id=v.toVariant().toString();if(!id.isEmpty()&&ids.size()<4)ids.append(id);}
    *out=QJsonObject{{"title",title},{"content",content},{"mood",moods.contains(i.value("mood").toString())?i.value("mood").toString():QStringLiteral("warm")},{"dream_type",types.contains(i.value("dream_type").toString())?i.value("dream_type").toString():QStringLiteral("random_fantasy")},{"symbols",symbols},{"color",color},{"reality_hint",i.value("reality_hint").toString().trimmed().left(160)},{"continuation_key",i.value("continuation_key").toString().trimmed().left(60)},{"memory_ids",ids}};
    return true;
}
