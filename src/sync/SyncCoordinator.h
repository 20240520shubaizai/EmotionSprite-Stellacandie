#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QJsonObject>
#include <QStringList>

#include "../data/Records.h"

class AgentClient;
class SyncRepository;

class SyncCoordinator final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool masterEnabled READ masterEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool settingsEnabled READ settingsEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool petStateEnabled READ petStateEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool memoryEnabled READ memoryEnabled NOTIFY statusChanged)
    Q_PROPERTY(bool reminderEnabled READ reminderEnabled NOTIFY statusChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY statusChanged)
    Q_PROPERTY(QString lastSuccess READ lastSuccess NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString lastOperation READ lastOperation NOTIFY statusChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY statusChanged)
    Q_PROPERTY(QStringList conflictItems READ conflictItems NOTIFY statusChanged)
    Q_PROPERTY(QString cloudUrl READ cloudUrl NOTIFY statusChanged)
    Q_PROPERTY(QString cloudConnectionStatus READ cloudConnectionStatus NOTIFY statusChanged)
    Q_PROPERTY(bool cloudConfigured READ cloudConfigured NOTIFY statusChanged)
public:
    SyncCoordinator(SyncRepository *repository,AgentClient *client,QObject *parent=nullptr);
    void start();
    void stop();
    bool masterEnabled()const;bool settingsEnabled()const;bool petStateEnabled()const;bool memoryEnabled()const;bool reminderEnabled()const;
    int pendingCount()const{return m_pendingCount;}QString lastSuccess()const{return m_lastSuccess;}QString lastError()const{return m_lastError;}QString lastOperation()const{return m_lastOperation;}QString deviceId()const;QStringList conflictItems()const{return m_conflictItems;}
    QString cloudUrl()const;QString cloudConnectionStatus()const;bool cloudConfigured()const;
    Q_INVOKABLE void setMasterEnabled(bool enabled);Q_INVOKABLE void setCategoryEnabled(const QString&type,bool enabled);
    Q_INVOKABLE void refreshStatus();Q_INVOKABLE void refreshConflicts();Q_INVOKABLE void resolveConflict(int row,const QString&choice);
    Q_INVOKABLE void exportCloudData();Q_INVOKABLE void deleteCloudData(const QString&confirmation);
    Q_INVOKABLE void saveCloudConfiguration(const QString&url,const QString&token);Q_INVOKABLE void clearCloudConfiguration();Q_INVOKABLE void testCloudConnection();
signals:void statusChanged();void cloudExportReady(const QJsonObject &data);void cloudDeletionFinished(int count);
private:
    void flush();
    void handleSuccess(const QJsonArray &results);
    void handleFailure(const QString &errorCode);
    SyncRepository *m_repository=nullptr;
    AgentClient *m_client=nullptr;
    QTimer m_timer;
    QList<SyncOutboxRecord> m_inFlight;
    int m_pendingCount=0;QString m_lastSuccess,m_lastError,m_lastOperation;QStringList m_conflictItems;QList<qint64> m_conflictIds;
};
