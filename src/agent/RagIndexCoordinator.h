#pragma once

#include <QObject>
#include <QJsonArray>
#include <QTimer>

class AgentClient;
class StorageService;

class RagIndexCoordinator final:public QObject
{
    Q_OBJECT
public:
    RagIndexCoordinator(StorageService *storage,AgentClient *client,QObject *parent=nullptr);
public slots:
    void scheduleRebuild();
private slots:
    void rebuildNow();
private:
    QJsonArray documents()const;
    StorageService *m_storage=nullptr;
    AgentClient *m_client=nullptr;
    QTimer m_debounce;
    QString m_requestId;
};
