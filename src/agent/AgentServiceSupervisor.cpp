#include "AgentServiceSupervisor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QUuid>
#include <QSettings>
#include <QStandardPaths>
#include "../AiCredentialStore.h"

AgentServiceSupervisor::AgentServiceSupervisor(QObject *parent):QObject(parent)
{
    m_readyTimer.setInterval(150);connect(&m_readyTimer,&QTimer::timeout,this,[this]{if(++m_readyChecks>300){m_readyTimer.stop();
        if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(1000))m_process.kill();}
        emit serviceUnavailable(QStringLiteral("Agent服务启动超时，正在按策略重启"));return;}
        auto *network=new QNetworkAccessManager(this);auto *reply=network->get(QNetworkRequest(m_endpoint.resolved(QUrl(QStringLiteral("/health")))));
        connect(reply,&QNetworkReply::finished,this,[this,reply,network]{if(reply->error()==QNetworkReply::NoError&&!m_readyEmitted){m_readyEmitted=true;m_readyTimer.stop();m_restartAttempt=0;emit serviceStarted(m_endpoint,m_token);}reply->deleteLater();network->deleteLater();});});
    connect(&m_process,&QProcess::finished,this,[this](int,QProcess::ExitStatus){m_readyTimer.stop();if(m_stopping)return;if(m_restartAttempt<3){++m_restartAttempt;emit serviceRestarting(m_restartAttempt);QTimer::singleShot(250*m_restartAttempt,this,&AgentServiceSupervisor::launch);}else emit serviceUnavailable(QStringLiteral("Agent服务反复退出，已进入离线模式"));});
}

AgentServiceSupervisor::~AgentServiceSupervisor(){stop();}
QString AgentServiceSupervisor::agentCoreDirectory()const{const QDir app(QCoreApplication::applicationDirPath());if(QFileInfo::exists(app.filePath(QStringLiteral("agent-runtime/agent-core.exe"))))return app.absolutePath();const QStringList candidates{app.filePath(QStringLiteral("agent-core")),app.filePath(QStringLiteral("../../agent-core"))};for(const auto&p:candidates)if(QFileInfo::exists(QDir(p).filePath(QStringLiteral("agent_core/__main__.py"))))return QDir(p).absolutePath();return {};}
QString AgentServiceSupervisor::pythonExecutable()const{const QDir app(QCoreApplication::applicationDirPath());const QString core=agentCoreDirectory();const QStringList candidates{app.filePath(QStringLiteral("agent-runtime/agent-core.exe")),app.filePath(QStringLiteral("agent-runtime/python.exe")),QDir(core).filePath(QStringLiteral(".venv/Scripts/python.exe"))};for(const auto&p:candidates)if(QFileInfo::exists(p))return QDir::toNativeSeparators(p);return {};}
quint16 AgentServiceSupervisor::reservePort()const{QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 0;return server.serverPort();}
void AgentServiceSupervisor::start(){if(m_process.state()!=QProcess::NotRunning)return;m_stopping=false;m_restartAttempt=0;launch();}
void AgentServiceSupervisor::restart(){stop();m_stopping=false;m_restartAttempt=0;launch();}
void AgentServiceSupervisor::launch()
{
    const QString python=pythonExecutable(),core=agentCoreDirectory();const quint16 port=reservePort();
    if(python.isEmpty()||core.isEmpty()||!port){emit serviceUnavailable(QStringLiteral("Agent运行环境不完整，已使用离线模式"));return;}
    m_readyEmitted=false;m_token=QUuid::createUuid().toString(QUuid::WithoutBraces)+QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_endpoint=QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));m_process.setWorkingDirectory(core);m_process.setProgram(python);
    QProcessEnvironment environment=QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("AGENT_SESSION_TOKEN"),m_token);
    QSettings settings;
    environment.insert(QStringLiteral("DEEPSEEK_BASE_URL"),settings.value(QStringLiteral("ai/baseUrl"),QStringLiteral("https://api.deepseek.com")).toString());
    environment.insert(QStringLiteral("DEEPSEEK_MODEL"),settings.value(QStringLiteral("ai/model"),QStringLiteral("deepseek-v4-flash")).toString());
    environment.insert(QStringLiteral("DEEPSEEK_API_KEY"),AiCredentialStore::loadApiKey());
    const QString localDataDirectory=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(localDataDirectory);
    if(environment.value(QStringLiteral("SYNC_DATABASE_URL")).isEmpty()){
        const QString syncDatabase=QDir(localDataDirectory).filePath(QStringLiteral("agent_core_sync.db"));
        environment.insert(QStringLiteral("SYNC_DATABASE_URL"),QStringLiteral("sqlite+pysqlite:///%1").arg(QDir::fromNativeSeparators(syncDatabase)));
    }
    const QString traceDirectory=QDir(localDataDirectory).filePath(QStringLiteral("observability"));
    QDir().mkpath(traceDirectory);environment.insert(QStringLiteral("AGENT_TRACE_PATH"),QDir(traceDirectory).filePath(QStringLiteral("traces.jsonl")));
    environment.insert(QStringLiteral("AGENT_TRACE_RETENTION_DAYS"),QStringLiteral("14"));environment.insert(QStringLiteral("AGENT_TRACE_MAX_COUNT"),QStringLiteral("300"));
    if(environment.value(QStringLiteral("AGENT_MODEL_MODE")).isEmpty())
        environment.insert(QStringLiteral("AGENT_MODEL_MODE"),QStringLiteral("real"));
    m_process.setProcessEnvironment(environment);
    const bool standalone=QFileInfo(python).fileName().compare(QStringLiteral("agent-core.exe"),Qt::CaseInsensitive)==0;
    m_process.setArguments(standalone?QStringList{QStringLiteral("--port"),QString::number(port)}:QStringList{QStringLiteral("-m"),QStringLiteral("agent_core"),QStringLiteral("--port"),QString::number(port)});
    const QString diagnosticLog=environment.value(QStringLiteral("AGENT_SUPERVISOR_LOG"));
    if(diagnosticLog.isEmpty()){m_process.setStandardOutputFile(QProcess::nullDevice());m_process.setStandardErrorFile(QProcess::nullDevice());}
    else{m_process.setStandardOutputFile(diagnosticLog,QIODevice::Append);m_process.setStandardErrorFile(diagnosticLog,QIODevice::Append);}
    m_process.start();if(!m_process.waitForStarted(3000)){emit serviceUnavailable(QStringLiteral("Agent服务无法启动，已使用离线模式"));return;}
    m_readyChecks=0;m_readyTimer.start();
}
void AgentServiceSupervisor::stop(){m_stopping=true;m_readyTimer.stop();if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(1500)){m_process.kill();m_process.waitForFinished(1000);}}}
