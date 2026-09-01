#include "ChatAgentAdapter.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>

ChatAgentAdapter::ChatAgentAdapter(AgentClient *client, QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(m_client, &AgentClient::requestFinished, this,
            [this](const QString &requestId, const QJsonObject &result) {
        if (!m_pending.contains(requestId)) return;
        const QString body = result.value(QStringLiteral("body")).toString().trimmed();
        const QString emotion = result.value(QStringLiteral("emotion")).toString();
        const QString traceId = result.value(QStringLiteral("trace_id")).toString();
        const QString errorCode = result.value(QStringLiteral("error_code")).toString();
        if (!errorCode.isEmpty()) {
            m_pending.remove(requestId);
            emit failed(requestId, errorCode, QStringLiteral("Agent模型调用失败"));
            return;
        }
        static const QSet<QString> emotions{QStringLiteral("neutral"), QStringLiteral("warm"),
            QStringLiteral("curious"), QStringLiteral("concerned")};
        if (body.isEmpty() || body.size() > 500 || !emotions.contains(emotion)) {
            m_pending.remove(requestId);
            emit failed(requestId, QStringLiteral("invalid_agent_reply"),
                        QStringLiteral("Agent回复未通过客户端校验"));
            return;
        }
        const QJsonObject effect = result.value(QStringLiteral("state_effect")).toObject();
        m_pending.remove(requestId);
        emit completed(requestId, body, emotion, effect, traceId,result.value(QStringLiteral("mutations")).toArray());
    });
    connect(m_client, &AgentClient::requestFailed, this,
            [this](const QString &requestId, const QString &code, const QString &message) {
        if (!m_pending.remove(requestId)) return;
        emit failed(requestId, code, message);
    });
}

QString ChatAgentAdapter::sendText(const QString &text, const QList<ChatMessageRecord> &history,
                                   const QString &personaContext, const QJsonObject &petState,
                                   bool allowMemory, const QString &attachmentName)
{
    QJsonArray recent;
    const int begin = qMax(0, history.size() - 20);
    for (int i = begin; i < history.size(); ++i) {
        const auto &item = history.at(i);
        recent.append(QJsonObject{{QStringLiteral("role"), item.sender == QStringLiteral("user")
                ? QStringLiteral("user") : QStringLiteral("assistant")},
            {QStringLiteral("content"), item.text.left(4000)},
            {QStringLiteral("created_at"), item.createdAt.toString(Qt::ISODateWithMs)}});
    }
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    QJsonObject payload{{QStringLiteral("schema_version"), QStringLiteral("conversation_v2")},
        {QStringLiteral("text"), text}, {QStringLiteral("current_time"), now},
        {QStringLiteral("conversation_context"), recent},
        {QStringLiteral("persona_context"), personaContext.left(16000)},
        {QStringLiteral("pet_state"), petState},
        {QStringLiteral("privacy"), QJsonObject{{QStringLiteral("allow_memory"), allowMemory},
            {QStringLiteral("allow_secret"), false}, {QStringLiteral("allow_cloud_sync"), false}}},
        {QStringLiteral("allowed_mutations"), QJsonArray{QStringLiteral("state_delta"),QStringLiteral("reminder"),QStringLiteral("memory_candidate")}}};
    payload.insert(QStringLiteral("attachment"), attachmentName.isEmpty() ? QJsonValue(QJsonValue::Null)
        : QJsonValue(QJsonObject{{QStringLiteral("name"), attachmentName},
                                 {QStringLiteral("kind"), QStringLiteral("image_description")}}));
    const QString requestId = m_client->submit(QStringLiteral("conversation_v2"), payload, 45000,
                                               QStringLiteral("user_chat"));
    m_pending.insert(requestId, RequestContext{requestId, text, now, attachmentName, allowMemory});
    return requestId;
}
