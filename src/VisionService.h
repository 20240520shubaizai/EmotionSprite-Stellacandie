#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>

class QNetworkReply;
class QNetworkRequest;
class VisionService final:public QObject
{
    Q_OBJECT
public:
    explicit VisionService(QObject*parent=nullptr);
    void configure(const QString&baseUrl,const QString&model,const QString&apiKey);
    bool isConfigured()const{return !m_apiKey.isEmpty()&&!m_baseUrl.isEmpty()&&!m_model.isEmpty();}
    bool busy()const{return m_busy;}QString baseUrl()const{return m_baseUrl;}QString model()const{return m_model;}
    void testConnection();
    void analyzeDreamPhoto(const QString&path,const QString&dreamTitle,const QString&dreamContent,const QString&userNote);
    void analyzeChatPhoto(const QString&path,const QString&userNote,const QString&recentDreams);
signals:
    void busyChanged();
    void testFinished(bool success,const QString&message);
    void analysisCompleted(const QJsonObject&result,const QString&userNote);
    void analysisFailed(const QString&message);
private:
    QNetworkRequest request(const QString&path)const;
    void setBusy(bool busy);
    QString networkError(QNetworkReply*reply)const;
    void analyzePhoto(const QString&path,const QString&prompt,const QString&userNote);
    QNetworkAccessManager m_network;QPointer<QNetworkReply>m_reply;QString m_baseUrl,m_model,m_apiKey;bool m_busy=false;
};
