#pragma once

#include "data/Records.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QJsonArray>
#include <QJsonObject>
#include "logic/LogicRoleManager.h"

class AiService final : public QObject
{
    Q_OBJECT

public:
    explicit AiService(QObject *parent = nullptr);

    void configure(const QString &baseUrl, const QString &model, const QString &apiKey);
    bool isConfigured() const;
    bool isBusy() const;
    QString baseUrl() const;
    QString model() const;
    bool runOutputValidationSelfTest(QStringList *failures = nullptr) const;

    void sendChat(const QList<ChatMessageRecord> &history,
                  int mood, int energy, int health, int closeness, int boredom, int neglect,
                  int curiosity, int irritation,
                  const QString &context = QStringLiteral("chat"), const QString &extraContext = QString());
    void testConnection();
    void analyzeMemories(const QList<ChatMessageRecord> &history);
    void summarizeText(const QString &text,const QString &mode,const QString &sourceName,
                       const QString &userInstruction = QString());
    void generateDream(const QJsonObject &dreamContext);
    void cancel();

signals:
    void busyChanged(bool busy);
    void chatCompleted(const QString &reply, const QString &emotion, const QJsonObject &stateEffect,
                       const QString &requestContext);
    void chatFailed(const QString &message, const QString &requestContext);
    void connectionTestFinished(bool success, const QString &message);
    void statusMessage(const QString &message);
    void memoryAnalysisCompleted(const QJsonArray &memories);
    void memoryAnalysisFailed(const QString &message);
    void summaryCompleted(const QJsonObject &summary);
    void summaryFailed(const QString &message);
    void dreamCompleted(const QJsonObject &dream);
    void dreamFailed(const QString &message);

private:
    QNetworkRequest makeRequest(const QString &path) const;
    QString loadSystemPrompt() const;
    QString friendlyNetworkError(QNetworkReply *reply) const;
    bool shouldRetry(QNetworkReply *reply) const;
    void sendChatRequest(const QByteArray &payload, const QString &requestContext,
                         int networkAttempt, int validationAttempt = 0);
    QStringList validateChatReply(const QString &reply, const QString &emotion, const QByteArray &requestPayload) const;
    QByteArray correctedRequestPayload(const QByteArray &requestPayload, const QStringList &issues) const;
    QByteArray relaxedResponsePayload(const QByteArray &requestPayload) const;
    QString safeFallbackReply(const QByteArray &requestPayload) const;
    void sendConnectionTest(int attempt);
    void setBusy(bool busy);

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_activeReply;
    QString m_baseUrl = QStringLiteral("https://api.deepseek.com");
    QString m_model = QStringLiteral("deepseek-v4-flash");
    QString m_apiKey;
    bool m_busy = false;
    LogicRoleManager m_logicRoles;
};
