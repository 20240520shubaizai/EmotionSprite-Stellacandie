#include "AgentClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUuid>

AgentClient::AgentClient(QObject *parent):QObject(parent){}

void AgentClient::configure(const QUrl &baseUrl,const QString &sessionToken)
{
    m_baseUrl=baseUrl;m_token=sessionToken;setState(QStringLiteral("connecting"),QStringLiteral("正在连接Agent服务"));checkCompatibility();
}

QNetworkRequest AgentClient::request(const QString &path) const
{
    QNetworkRequest req(m_baseUrl.resolved(QUrl(path)));req.setRawHeader("X-Session-Token",m_token.toUtf8());
    req.setRawHeader("X-Client-Version","1.0.0");req.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/json"));return req;
}

void AgentClient::setState(const QString &state,const QString &text)
{if(m_state==state&&m_statusText==text)return;m_state=state;m_statusText=text;emit stateChanged();}

void AgentClient::checkCompatibility()
{
    if(!m_baseUrl.isValid()||m_token.isEmpty()){setState(QStringLiteral("offline"),QStringLiteral("Agent服务未配置"));return;}
    auto *reply=m_network.get(request(QStringLiteral("/v1/capabilities")));
    connect(reply,&QNetworkReply::finished,this,[this,reply]{const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()==QNetworkReply::NoError&&code==200){const auto obj=QJsonDocument::fromJson(reply->readAll()).object();
            if(obj.value(QStringLiteral("protocol_version")).toString().startsWith(QStringLiteral("1.")))setState(QStringLiteral("online"),QStringLiteral("Agent服务在线"));
            else setState(QStringLiteral("incompatible"),QStringLiteral("Agent协议版本不兼容"));}
        else setState(QStringLiteral("offline"),code==426?QStringLiteral("Agent客户端版本不兼容"):QStringLiteral("Agent服务暂不可用，已使用离线降级"));reply->deleteLater();});
}

QString AgentClient::submit(const QString &operation,const QJsonObject &payload,int timeoutMs,const QString &taskClass)
{
    const QString requestId=QUuid::createUuid().toString(QUuid::WithoutBraces),traceId=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(!available()){QTimer::singleShot(0,this,[this,requestId]{emit requestFailed(requestId,QStringLiteral("agent_offline"),QStringLiteral("Agent服务不可用"));});return requestId;}
    const QString normalizedClass=taskClass==QStringLiteral("background")?QStringLiteral("background"):QStringLiteral("user_chat");
    const QJsonObject body{{QStringLiteral("request_id"),requestId},{QStringLiteral("trace_id"),traceId},{QStringLiteral("operation"),operation},
        {QStringLiteral("payload"),payload},{QStringLiteral("timeout_ms"),qBound(100,timeoutMs,120000)},
        {QStringLiteral("max_retries"),1},{QStringLiteral("task_class"),normalizedClass}};
    auto *reply=m_network.post(request(QStringLiteral("/v1/tasks")),QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply,requestId]{const auto obj=QJsonDocument::fromJson(reply->readAll()).object();
        if(reply->error()==QNetworkReply::NoError&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==202)
            openStream(requestId,obj.value(QStringLiteral("stream_url")).toString());
        else emit requestFailed(requestId,QStringLiteral("submit_failed"),QStringLiteral("Agent请求提交失败"));reply->deleteLater();});return requestId;
}

void AgentClient::openStream(const QString &requestId,const QString &path,int after,int reconnects)
{
    QString url=path;if(after>0)url+=QStringLiteral("?last_event_id=%1").arg(after);
    // SSE responses can remain open briefly after their terminal event.  Give each
    // task its own connection pool so a stale stream can never starve normal HTTP
    // requests or the next task stream.
    auto *streamNetwork=new QNetworkAccessManager(this);
    auto streamRequest=request(url);
    // A real model call may take longer than the keepalive interval.  The server
    // sends SSE keepalives, so this timeout protects dead connections without
    // aborting healthy DeepSeek requests.
    streamRequest.setTransferTimeout(35000);
    auto *reply=streamNetwork->get(streamRequest);m_buffers.insert(reply,{});
    connect(reply,&QIODevice::readyRead,this,[this,reply,requestId]{m_buffers[reply]+=reply->readAll();parseSse(reply,requestId);});
    connect(reply,&QNetworkReply::finished,this,[this,reply,streamNetwork,requestId,path,reconnects]{m_buffers[reply]+=reply->readAll();parseSse(reply,requestId);
        const bool terminal=reply->property("terminal").toBool();m_buffers.remove(reply);const auto error=reply->error();reply->deleteLater();streamNetwork->deleteLater();
        if(!terminal&&error!=QNetworkReply::OperationCanceledError&&reconnects<2){const int after=m_lastSequence.value(requestId);QTimer::singleShot(150*(reconnects+1),this,[=]{openStream(requestId,path,after,reconnects+1);});}
        else if(!terminal&&error!=QNetworkReply::OperationCanceledError)emit requestFailed(requestId,QStringLiteral("stream_interrupted"),QStringLiteral("Agent事件流中断"));});
}

void AgentClient::parseSse(QNetworkReply *reply,const QString &requestId)
{
    QByteArray &buffer=m_buffers[reply];int split;
    while((split=buffer.indexOf("\n\n"))>=0){const QByteArray block=buffer.left(split);buffer.remove(0,split+2);int sequence=0;QString event;QByteArray data;
        for(const QByteArray &line:block.split('\n')){if(line.startsWith("id: "))sequence=line.mid(4).toInt();else if(line.startsWith("event: "))event=QString::fromUtf8(line.mid(7));else if(line.startsWith("data: "))data=line.mid(6);}
        if(sequence<=m_lastSequence.value(requestId)||data.isEmpty())continue;m_lastSequence[requestId]=sequence;const auto root=QJsonDocument::fromJson(data).object();const auto body=root.value(QStringLiteral("data")).toObject();
        emit requestEvent(requestId,event,body,sequence);if(event==QStringLiteral("result")){reply->setProperty("terminal",true);emit requestFinished(requestId,body);QTimer::singleShot(0,reply,[reply]{if(reply->isRunning())reply->abort();});}
        else if(event==QStringLiteral("failed")||event==QStringLiteral("cancelled")){reply->setProperty("terminal",true);const auto error=root.value(QStringLiteral("error")).toObject();emit requestFailed(requestId,error.value(QStringLiteral("code")).toString(),error.value(QStringLiteral("message")).toString());QTimer::singleShot(0,reply,[reply]{if(reply->isRunning())reply->abort();});}}
}

void AgentClient::cancel(const QString &requestId)
{auto *reply=m_network.deleteResource(request(QStringLiteral("/v1/tasks/%1").arg(requestId)));connect(reply,&QNetworkReply::finished,reply,&QObject::deleteLater);}

void AgentClient::submitSyncBatch(const QJsonArray &events)
{
    if(!available()){emit syncBatchFailed(QStringLiteral("agent_offline"));return;}
    auto *reply=m_network.post(request(QStringLiteral("/v1/sync/batch")),
                               QJsonDocument(QJsonObject{{QStringLiteral("events"),events}}).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject body=QJsonDocument::fromJson(reply->readAll()).object();
        if(reply->error()==QNetworkReply::NoError&&status==200)
            emit syncBatchFinished(body.value(QStringLiteral("results")).toArray());
        else
            emit syncBatchFailed(status>0?QStringLiteral("http_%1").arg(status):QStringLiteral("network_error"));
        reply->deleteLater();
    });
}

void AgentClient::fetchSyncConflicts()
{
    if(!available()){emit syncControlFailed(QStringLiteral("agent_offline"));return;}
    auto *reply=m_network.get(request(QStringLiteral("/v1/sync/conflicts?user_id=local-single-user")));
    connect(reply,&QNetworkReply::finished,this,[this,reply]{const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto body=QJsonDocument::fromJson(reply->readAll()).object();if(reply->error()==QNetworkReply::NoError&&status==200)emit syncConflictsFinished(body.value("conflicts").toArray());else emit syncControlFailed(QStringLiteral("conflicts_http_%1").arg(status));reply->deleteLater();});
}

void AgentClient::resolveSyncConflict(qint64 conflictId,const QString &choice)
{
    if(!available()||(choice!=QStringLiteral("local")&&choice!=QStringLiteral("cloud"))){emit syncControlFailed(QStringLiteral("invalid_resolution"));return;}
    auto *reply=m_network.post(request(QStringLiteral("/v1/sync/conflicts/%1/resolve").arg(conflictId)),QJsonDocument(QJsonObject{{QStringLiteral("choice"),choice},{QStringLiteral("user_id"),QStringLiteral("local-single-user")}}).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[this,reply,conflictId]{const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto body=QJsonDocument::fromJson(reply->readAll()).object();if(reply->error()==QNetworkReply::NoError&&status==200)emit syncConflictResolved(conflictId,body.value("status").toString());else emit syncControlFailed(QStringLiteral("resolve_http_%1").arg(status));reply->deleteLater();});
}

void AgentClient::requestCloudExport()
{
    if(!available()){emit syncControlFailed(QStringLiteral("agent_offline"));return;}auto *reply=m_network.get(request(QStringLiteral("/v1/sync/export?user_id=local-single-user")));connect(reply,&QNetworkReply::finished,this,[this,reply]{const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto body=QJsonDocument::fromJson(reply->readAll()).object();if(reply->error()==QNetworkReply::NoError&&status==200)emit cloudExportReady(body);else emit syncControlFailed(QStringLiteral("export_http_%1").arg(status));reply->deleteLater();});
}

void AgentClient::requestCloudDeletion(const QString &confirmation)
{
    if(!available()){emit syncControlFailed(QStringLiteral("agent_offline"));return;}auto *reply=m_network.post(request(QStringLiteral("/v1/sync/delete-cloud-data")),QJsonDocument(QJsonObject{{QStringLiteral("user_id"),QStringLiteral("local-single-user")},{QStringLiteral("confirmation"),confirmation}}).toJson(QJsonDocument::Compact));connect(reply,&QNetworkReply::finished,this,[this,reply]{const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto body=QJsonDocument::fromJson(reply->readAll()).object();if(reply->error()==QNetworkReply::NoError&&status==200)emit cloudDeletionFinished(body.value("deleted_count").toInt());else emit syncControlFailed(QStringLiteral("delete_http_%1").arg(status));reply->deleteLater();});
}
