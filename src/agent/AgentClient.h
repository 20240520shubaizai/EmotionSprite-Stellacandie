#pragma once

#include <QObject>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkReply;

class AgentClient final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
public:
    explicit AgentClient(QObject *parent=nullptr);
    QString state() const{return m_state;}
    QString statusText() const{return m_statusText;}
    bool available() const{return m_state==QStringLiteral("online");}
    void configure(const QUrl &baseUrl,const QString &sessionToken);
    Q_INVOKABLE void checkCompatibility();
    Q_INVOKABLE QString submit(const QString &operation,const QJsonObject &payload,int timeoutMs=30000,
                               const QString &taskClass=QStringLiteral("user_chat"));
    Q_INVOKABLE void cancel(const QString &requestId);
    void submitSyncBatch(const QJsonArray &events);
    Q_INVOKABLE void fetchSyncConflicts();
    Q_INVOKABLE void resolveSyncConflict(qint64 conflictId,const QString &choice);
    Q_INVOKABLE void requestCloudExport();
    Q_INVOKABLE void requestCloudDeletion(const QString &confirmation);
signals:
    void stateChanged();
    void requestEvent(const QString &requestId,const QString &event,const QJsonObject &data,int sequence);
    void requestFinished(const QString &requestId,const QJsonObject &result);
    void requestFailed(const QString &requestId,const QString &code,const QString &message);
    void syncBatchFinished(const QJsonArray &results);
    void syncBatchFailed(const QString &errorCode);
    void syncConflictsFinished(const QJsonArray &conflicts);
    void syncConflictResolved(qint64 conflictId,const QString &status);
    void syncControlFailed(const QString &errorCode);
    void cloudExportReady(const QJsonObject &data);
    void cloudDeletionFinished(int deletedCount);
private:
    QNetworkRequest request(const QString &path) const;
    void setState(const QString &state,const QString &text);
    void openStream(const QString &requestId,const QString &path,int after=0,int reconnects=0);
    void parseSse(QNetworkReply *reply,const QString &requestId);
    QNetworkAccessManager m_network;
    QUrl m_baseUrl;
    QString m_token,m_state=QStringLiteral("offline"),m_statusText=QStringLiteral("Agent服务未连接");
    QHash<QString,int> m_lastSequence;
    QHash<QNetworkReply*,QByteArray> m_buffers;
};
