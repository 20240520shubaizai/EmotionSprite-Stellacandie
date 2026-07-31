#include "AdaptiveLearningModule.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

AdaptiveLearningModule::AdaptiveLearningModule(QObject *parent) : FeatureModule(parent)
{
    QSettings settings;
    m_enabled = settings.value("modules/adaptiveLearningEnabled", true).toBool();
    const QDate last = QDate::fromString(settings.value("learning/lastDecay").toString(), Qt::ISODate);
    if (!last.isValid() || last.daysTo(QDate::currentDate()) >= 30) {
        for (const QString &key : {"humor", "followup", "detail", "advice", "affection"})
            settings.setValue("learning/score/" + key, settings.value("learning/score/" + key, 0.5).toDouble() * 0.9);
        settings.setValue("learning/lastDecay", QDate::currentDate().toString(Qt::ISODate));
    }
}

QString AdaptiveLearningModule::id() const { return "adaptive_learning"; }
QString AdaptiveLearningModule::displayName() const { return QStringLiteral("启发式学习"); }
bool AdaptiveLearningModule::isEnabled() const { return m_enabled; }
void AdaptiveLearningModule::setEnabled(bool enabled)
{
    m_enabled = enabled;
    QSettings().setValue("modules/adaptiveLearningEnabled", enabled);
}

double AdaptiveLearningModule::score(const QString &key) const
{
    return QSettings().value("learning/score/" + key, 0.5).toDouble();
}

void AdaptiveLearningModule::adjust(const QString &key, double delta, const QString &reason)
{
    QSettings settings;
    const double value = qBound(0.0, settings.value("learning/score/" + key, 0.5).toDouble() + delta, 1.0);
    settings.setValue("learning/score/" + key, value);
    QJsonArray log = QJsonDocument::fromJson(settings.value("learning/log").toByteArray()).array();
    log.append(QJsonObject{{"at", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
                           {"dimension", key}, {"delta", delta}, {"reason", reason}});
    while (log.size() > 100) log.removeFirst();
    settings.setValue("learning/log", QJsonDocument(log).toJson(QJsonDocument::Compact));
}

void AdaptiveLearningModule::observeUserResponse(const QString &message, const QString &lastReply)
{
    if (!m_enabled || lastReply.isEmpty()) return;
    const bool positive = message.contains(QStringLiteral("哈哈")) || message.contains(QStringLiteral("笑死"))
        || message.contains(QStringLiteral("你懂")) || message.contains(QStringLiteral("有意思"))
        || message.contains(QStringLiteral("不错"));
    const bool negative = message.contains(QStringLiteral("不是这个意思")) || message.contains(QStringLiteral("别这么说"))
        || message.contains(QStringLiteral("尴尬")) || message.contains(QStringLiteral("别玩梗"));
    const bool humorousReply = lastReply.contains(QStringLiteral("哈哈")) || lastReply.contains(QStringLiteral("梗"))
        || lastReply.contains(QStringLiteral("笑"));
    if (positive && humorousReply) adjust("humor", 0.05, QStringLiteral("用户认可幽默表达"));
    if (negative) adjust("humor", -0.12, QStringLiteral("用户纠正表达方式"));
    if (message.contains(QStringLiteral("别问")) || message.contains(QStringLiteral("不想说")))
        adjust("followup", -0.10, QStringLiteral("用户拒绝追问"));
    else if (message.size() > 20) adjust("followup", 0.02, QStringLiteral("用户继续详细讲述"));
    if (message.contains(QStringLiteral("别建议")) || message.contains(QStringLiteral("不用教我")))
        adjust("advice", -0.12, QStringLiteral("用户拒绝建议"));
    if (message.contains(QStringLiteral("详细点"))) adjust("detail", 0.08, QStringLiteral("用户要求更详细"));
    if (message.contains(QStringLiteral("简单点")) || message.contains(QStringLiteral("说短点")))
        adjust("detail", -0.08, QStringLiteral("用户要求更简短"));
}

QString AdaptiveLearningModule::context() const
{
    if (!m_enabled) return {};
    return QStringLiteral(
        "[ADAPTIVE_PREFERENCES]\n这些是缓慢学习的软偏好，不得凌驾于用户本轮明确要求、角色圣经和安全边界："
        "幽默/玩梗=%1，追问=%2，详细度=%3，主动建议=%4，亲昵=%5。"
        "高于0.65可适度增加，低于0.35应减少；不要向用户念出分数。")
        .arg(score("humor"), 0, 'f', 2).arg(score("followup"), 0, 'f', 2)
        .arg(score("detail"), 0, 'f', 2).arg(score("advice"), 0, 'f', 2)
        .arg(score("affection"), 0, 'f', 2);
}
