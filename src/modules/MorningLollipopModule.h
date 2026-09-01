#pragma once

#include "FeatureModule.h"
#include "../data/repositories/MorningLollipopRepository.h"
#include <QTimer>
#include <QNetworkAccessManager>

class AiService;

class MorningLollipopModule final : public FeatureModule
{
    Q_OBJECT
public:
    MorningLollipopModule(MorningLollipopRepository*,AiService*,QObject *parent=nullptr);
    QString id()const override{return QStringLiteral("morning_lollipop");}
    QString displayName()const override{return QStringLiteral("晨间棒棒糖");}
    bool isEnabled()const override{return m_enabled;}
    void setEnabled(bool enabled)override;
    int workdayStart()const{return m_workdayStart;} int workdayEnd()const{return m_workdayEnd;}
    int weekendStart()const{return m_weekendStart;} int weekendEnd()const{return m_weekendEnd;}
    QString status()const{return m_status;} QStringList items()const; MorningLollipopRecord selected()const;
    QString city()const{return m_city;} QString weatherText()const{return m_weatherText;}
    void saveWindows(int workStart,int workEnd,int weekendStart,int weekendEnd);
    void select(int row); void toggleFavorite(); void evaluateNow(); void createTestGift();
    void setCity(const QString&city); void refreshWeather();
    MorningLollipopRecord planForTest(const QDate&date)const{return createPlan(QDateTime(date,QTime(8,0)));}
signals:
    void changed(); void enabledChanged(bool enabled);
    void giftReady(const QString &title,const QString &message);
private:
    struct Flavor{QString id,name,category,emoji,color,rarity;};
    QList<Flavor> catalog()const; MorningLollipopRecord createPlan(const QDateTime&now)const;
    QString offlineGreeting(const MorningLollipopRecord&r)const; QString fingerprint(const QString&s)const;
    bool isFullScreenActive()const; int idleSeconds()const; bool isDnd()const;
    void refresh(); void prepareGreeting(MorningLollipopRecord record); void deliver(MorningLollipopRecord record,const QDateTime&now,bool test=false);
    bool applyMemorial(MorningLollipopRecord&r,const QDateTime&now)const; void setPickingStage(const QString&stage,const QString&line);
    QString weatherDeliveryLine()const;
    MorningLollipopRepository*m_storage=nullptr;AiService*m_ai=nullptr;QTimer m_timer;
    bool m_enabled=true,m_preparing=false; int m_workdayStart=450,m_workdayEnd=600,m_weekendStart=540,m_weekendEnd=690;
    QString m_status; QList<MorningLollipopRecord>m_items; int m_selected=0;
    QNetworkAccessManager m_weatherNetwork; QString m_city,m_weatherType=QStringLiteral("unknown"),m_weatherText=QStringLiteral("天气尚未同步");
    QDateTime m_lastWeatherSync; bool m_weatherRefreshing=false;
};
