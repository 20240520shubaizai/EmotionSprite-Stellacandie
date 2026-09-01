#pragma once

#include "AgentClient.h"
#include "../data/Records.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>

class ChatAgentAdapter final : public QObject
{
    Q_OBJECT
public:
    struct RequestContext {
        QString requestId;
        QString userText;
        QString createdAt;
        QString attachmentName;
        bool allowMemory = true;
    };

    explicit ChatAgentAdapter(AgentClient *client, QObject *parent = nullptr);
    QString sendText(const QString &text, const QList<ChatMessageRecord> &history,
                     const QString &personaContext, const QJsonObject &petState,
                     bool allowMemory = true, const QString &attachmentName = {});
    int pendingCount() const { return m_pending.size(); }

signals:
    void completed(const QString &requestId, const QString &body, const QString &emotion,
                   const QJsonObject &stateEffect, const QString &traceId,const QJsonArray &mutations);
    void failed(const QString &requestId, const QString &code, const QString &message);

private:
    AgentClient *m_client = nullptr;
    QHash<QString, RequestContext> m_pending;
};
