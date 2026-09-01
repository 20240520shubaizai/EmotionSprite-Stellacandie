#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>

class AgentServiceSupervisor final : public QObject
{
    Q_OBJECT
public:
    explicit AgentServiceSupervisor(QObject *parent=nullptr);
    ~AgentServiceSupervisor() override;
    void start();
    void stop();
    void restart();
    void simulateCrashForTest(){if(m_process.state()!=QProcess::NotRunning)m_process.kill();}
    QUrl endpoint() const{return m_endpoint;}
    QString sessionToken() const{return m_token;}
signals:
    void serviceStarted(const QUrl &endpoint,const QString &sessionToken);
    void serviceUnavailable(const QString &reason);
    void serviceRestarting(int attempt);
private:
    QString pythonExecutable() const;
    QString agentCoreDirectory() const;
    quint16 reservePort() const;
    void launch();
    QProcess m_process;
    QTimer m_readyTimer;
    QUrl m_endpoint;
    QString m_token;
    int m_restartAttempt=0,m_readyChecks=0;
    bool m_stopping=false,m_readyEmitted=false;
};
