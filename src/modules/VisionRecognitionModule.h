#pragma once
#include "FeatureModule.h"
#include <QJsonObject>
#include <QUrl>
class VisionService;
class VisionRecognitionModule final:public FeatureModule
{
    Q_OBJECT
public:
    explicit VisionRecognitionModule(VisionService*service,QObject*parent=nullptr);
    QString id()const override{return QStringLiteral("vision_recognition");}QString displayName()const override{return QStringLiteral("梦境识图");}
    bool isEnabled()const override{return m_enabled;}void setEnabled(bool enabled)override;
    bool preparePhoto(const QUrl&url);void clear();void analyze(const QString&dreamTitle,const QString&dreamContent,const QString&note);void analyzeChat(const QString&note,const QString&recentDreams);
    bool hasPhoto()const{return !m_path.isEmpty();}QString photoUrl()const{return m_path.isEmpty()?QString():QUrl::fromLocalFile(m_path).toString();}QString fileName()const;QString status()const{return m_status;}QString resultSummary()const{return m_resultSummary;}
signals:void changed();void enabledChanged(bool);void recognized(const QJsonObject&result,const QString&note);void chatRecognized(const QJsonObject&result,const QString&note,const QString&fileName);
private:VisionService*m_service=nullptr;bool m_enabled=true,m_chatMode=false;QString m_path,m_status,m_resultSummary;
};
