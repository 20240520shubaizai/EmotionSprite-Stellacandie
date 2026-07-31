#include "ProactiveBehaviorModule.h"
#include "../logic/CognitiveRoutingRole.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>

ProactiveBehaviorModule::ProactiveBehaviorModule(DataRepository *repository,QObject *parent)
    :FeatureModule(parent),m_repository(repository)
{
    QSettings s;m_enabled=s.value(QStringLiteral("modules/proactiveEnabled"),true).toBool();m_dnd=s.value(QStringLiteral("proactive/dnd"),false).toBool();
    m_dailyLimit=qBound(1,s.value(QStringLiteral("proactive/dailyLimit"),3).toInt(),6);m_quietStart=qBound(0,s.value(QStringLiteral("proactive/quietStart"),23).toInt(),23);m_quietEnd=qBound(0,s.value(QStringLiteral("proactive/quietEnd"),8).toInt(),23);
    m_repository->removeTimeBoundMemories();m_repository->cancelReminders(QStringLiteral("proactive_story_followup"));m_repository->archiveExpiredCognitiveRecords(QDateTime::currentDateTime());
    m_timer.setInterval(60*1000);connect(&m_timer,&QTimer::timeout,this,&ProactiveBehaviorModule::evaluateNow);m_timer.start();
    QTimer::singleShot(2500,this,&ProactiveBehaviorModule::evaluateNow);QTimer::singleShot(5000,this,&ProactiveBehaviorModule::seedLifestyleNudge);
}
QString ProactiveBehaviorModule::id()const{return QStringLiteral("proactive_behavior");}
QString ProactiveBehaviorModule::displayName()const{return QStringLiteral("认知与事务管理");}
bool ProactiveBehaviorModule::isEnabled()const{return m_enabled;}
void ProactiveBehaviorModule::setEnabled(bool v){if(v==m_enabled)return;m_enabled=v;QSettings().setValue(QStringLiteral("modules/proactiveEnabled"),v);emit enabledChanged(v);emit settingsChanged();}
bool ProactiveBehaviorModule::doNotDisturb()const{return m_dnd;}
void ProactiveBehaviorModule::setDoNotDisturb(bool v){if(v==m_dnd)return;m_dnd=v;QSettings().setValue(QStringLiteral("proactive/dnd"),v);emit settingsChanged();}
int ProactiveBehaviorModule::dailyLimit()const{return m_dailyLimit;}void ProactiveBehaviorModule::setDailyLimit(int v){v=qBound(1,v,6);if(v==m_dailyLimit)return;m_dailyLimit=v;QSettings().setValue(QStringLiteral("proactive/dailyLimit"),v);emit settingsChanged();}
int ProactiveBehaviorModule::quietStartHour()const{return m_quietStart;}int ProactiveBehaviorModule::quietEndHour()const{return m_quietEnd;}
void ProactiveBehaviorModule::setQuietHours(int a,int b){m_quietStart=qBound(0,a,23);m_quietEnd=qBound(0,b,23);QSettings s;s.setValue(QStringLiteral("proactive/quietStart"),m_quietStart);s.setValue(QStringLiteral("proactive/quietEnd"),m_quietEnd);emit settingsChanged();}
bool ProactiveBehaviorModule::isQuietTime(const QDateTime&now)const{const int h=now.time().hour();if(m_quietStart==m_quietEnd)return false;return m_quietStart>m_quietEnd?h>=m_quietStart||h<m_quietEnd:h>=m_quietStart&&h<m_quietEnd;}
bool ProactiveBehaviorModule::schedule(const QString&type,const QDateTime&when,const QString&message){return m_repository&&m_repository->addReminder(QStringLiteral("proactive_")+type,when,message)>0;}
void ProactiveBehaviorModule::acknowledgeUserResponse(){QSettings s;s.setValue(QStringLiteral("proactive/ignoredCount"),0);s.setValue(QStringLiteral("proactive/awaitingResponse"),false);}

CognitiveRecord ProactiveBehaviorModule::bestMatchingActive(const QString &subject)const
{
    const auto records=m_repository->loadCognitiveRecords({QStringLiteral("planned"),QStringLiteral("awaiting_confirmation"),QStringLiteral("awaiting_followup")});
    CognitiveRecord fallback;for(const auto&r:records){if(fallback.id==0)fallback=r;if(!subject.isEmpty()&&(r.subject.contains(subject,Qt::CaseInsensitive)||subject.contains(r.subject,Qt::CaseInsensitive)))return r;}return fallback;
}

qint64 ProactiveBehaviorModule::scheduleCognitive(CognitiveRecord r,const QString &payload)
{
    const qint64 id=m_repository->addCognitiveRecord(r);if(id<=0)return 0;
    if(r.status==QStringLiteral("planned")&&r.scheduledAt.isValid()&&r.explicitRequest){const qint64 reminder=m_repository->addReminder(QStringLiteral("cognitive_")+r.recordType,r.scheduledAt,payload);if(reminder>0)m_repository->updateCognitiveRecord(id,r.status,r.scheduledAt,r.followUpAt,r.followUpCount,reminder);}
    return id;
}

bool ProactiveBehaviorModule::handleUserMessage(const QString &message,QString *reply)
{
    if(!m_repository)return false;const QString text=message.trimmed();const QDateTime now=QDateTime::currentDateTime();
    if(text.contains(QStringLiteral("恢复提醒"))||text.contains(QStringLiteral("可以提醒我"))){setDoNotDisturb(false);if(reply)*reply=QStringLiteral("好，提醒恢复了。我仍会控制普通主动消息的频率。");return true;}
    if(text.contains(QStringLiteral("先别打扰"))||text.contains(QStringLiteral("想静一静"))){QSettings().setValue(QStringLiteral("proactive/snoozeUntil"),now.addSecs(3600).toString(Qt::ISODateWithMs));if(reply)*reply=QStringLiteral("好，我先安静一个小时。明确设置的到期提醒仍会保留。");return true;}

    // 先完成上一条缺少具体时刻的提醒。
    const auto awaiting=m_repository->loadCognitiveRecords({QStringLiteral("awaiting_confirmation")});
    if(!awaiting.isEmpty()){bool d=false,c=false;QDateTime at=CognitiveRoutingRole::parseTime(text,now,&d,&c);if(at.isValid()&&c){
        const CognitiveRecord pending=awaiting.first();if(!d&&pending.scheduledAt.isValid())at.setDate(pending.scheduledAt.date());const qint64 reminder=m_repository->addReminder(QStringLiteral("cognitive_")+pending.recordType,at,QStringLiteral("到时间啦：%1").arg(pending.subject));
        m_repository->updateCognitiveRecord(pending.id,QStringLiteral("planned"),at,QDateTime(),0,reminder);
        if(reply)*reply=QStringLiteral("好，时间补完整了：%1提醒你“%2”。").arg(at.toString(QStringLiteral("M月d日 HH:mm")),pending.subject);return true;}}

    const CognitiveRouteResult route=CognitiveRoutingRole::route(text,now);
    if(route.action.isEmpty())return false;
    if(route.action==QStringLiteral("cancel")||route.action==QStringLiteral("complete")){
        const CognitiveRecord item=bestMatchingActive(route.subject);if(item.id<=0){if(reply)*reply=QStringLiteral("我没有找到对应的未完成事项，你可以告诉我它的大概内容。");return true;}
        const QString state=route.action==QStringLiteral("cancel")?QStringLiteral("cancelled"):QStringLiteral("completed");m_repository->updateCognitiveRecord(item.id,state);if(item.reminderId>0)m_repository->updateReminderStatus(item.reminderId,QStringLiteral("cancelled"));
        if(reply)*reply=route.action==QStringLiteral("cancel")?QStringLiteral("好，这项提醒已经取消，我不会再追问。"):QStringLiteral("记下啦，这件事已经完成，我把它归档了。");return true;
    }
    if(route.action==QStringLiteral("reschedule")){
        const CognitiveRecord item=bestMatchingActive(route.subject);if(item.id<=0||!route.scheduledAt.isValid()){if(reply)*reply=QStringLiteral("我还没能确定要修改哪一项，或者新的时间不够明确。可以把事项和时间一起说一次吗？");return true;}
        if(item.reminderId>0)m_repository->updateReminderStatus(item.reminderId,QStringLiteral("cancelled"));const qint64 reminder=m_repository->addReminder(QStringLiteral("cognitive_")+item.recordType,route.scheduledAt,QStringLiteral("到时间啦：%1").arg(item.subject));m_repository->updateCognitiveRecord(item.id,QStringLiteral("planned"),route.scheduledAt,QDateTime(),0,reminder);if(reply)*reply=QStringLiteral("改好了，新的时间是%1。").arg(route.scheduledAt.toString(QStringLiteral("M月d日 HH:mm")));return true;
    }
    const CognitiveRecord duplicate=bestMatchingActive(route.subject);if(duplicate.id>0&&!route.subject.isEmpty()&&(duplicate.subject.contains(route.subject)||route.subject.contains(duplicate.subject))){if(reply)*reply=QStringLiteral("这件事我已经记着了，不会重复创建。如果要改时间，直接说“改到……”就好。");return true;}
    CognitiveRecord record;record.recordType=route.recordType;record.subject=route.subject;record.sourceText=text;record.scheduledAt=route.scheduledAt;record.explicitRequest=route.explicitReminder;record.deliveryPriority=route.deliveryPriority;record.memoryImportance=route.memoryImportance;record.followUpPolicy=route.followUpPolicy;record.maxFollowUps=route.maxFollowUps;
    if(route.needsTimeConfirmation){record.status=QStringLiteral("awaiting_confirmation");record.expiresAt=now.addDays(7);if(scheduleCognitive(record,{})>0){if(reply)*reply=QStringLiteral("这件事我已经先记下了，不过还缺具体时间。你希望我在那天几点提醒你？");return true;}}
    if(!record.scheduledAt.isValid()){if(reply)*reply=QStringLiteral("我听出这是一件日程，但还不知道具体什么时候发生。你愿意补充一下日期和时间吗？");return true;}
    record.status=QStringLiteral("planned");record.eventEndAt=record.scheduledAt.addSecs(2*3600);record.expiresAt=record.scheduledAt.addDays(record.recordType==QStringLiteral("event")?3:30);
    const QString payload=QStringLiteral("到时间啦：%1").arg(record.subject);if(scheduleCognitive(record,payload)>0){if(reply)*reply=record.explicitRequest?QStringLiteral("记好了，%1我会提醒你“%2”。").arg(record.scheduledAt.toString(QStringLiteral("M月d日 HH:mm")),record.subject):QStringLiteral("我记下这个日程了。除非你明确让我提醒，否则我只会把它当作近期事件。");return true;}
    return false;
}

void ProactiveBehaviorModule::evaluateNow()
{
    if(!m_repository||!m_enabled)return;const QDateTime now=QDateTime::currentDateTime();m_repository->archiveExpiredCognitiveRecords(now);
    const auto due=m_repository->loadDueReminders(now,30);QSettings s;const QDateTime snooze=QDateTime::fromString(s.value(QStringLiteral("proactive/snoozeUntil")).toString(),Qt::ISODateWithMs);
    const auto active=m_repository->loadCognitiveRecords({QStringLiteral("planned"),QStringLiteral("awaiting_followup")});
    for(const auto&r:due){if(!r.type.startsWith(QStringLiteral("cognitive_"))&&!r.type.startsWith(QStringLiteral("proactive_")))continue;CognitiveRecord cognitive;for(const auto&c:active)if(c.reminderId==r.id){cognitive=c;break;}const bool explicitTask=cognitive.id>0&&cognitive.explicitRequest;
        if(!explicitTask&&(m_dnd||isQuietTime(now)||(snooze.isValid()&&now<snooze)))continue;
        const QDateTime last=QDateTime::fromString(s.value(QStringLiteral("proactive/lastDeliveredAt")).toString(),Qt::ISODateWithMs);
        if(!explicitTask&&(m_repository->deliveredReminderCount(now.date(),QStringLiteral("proactive_"))>=m_dailyLimit||(last.isValid()&&last.secsTo(now)<3600)))continue;
        if(cognitive.recordType==QStringLiteral("event")&&r.scheduledAt.secsTo(now)>6*3600){m_repository->updateReminderStatus(r.id,QStringLiteral("missed"));QDateTime follow(now.date().addDays(1),QTime(10,0));if(follow<now)follow=now.addSecs(60);m_repository->updateCognitiveRecord(cognitive.id,QStringLiteral("awaiting_followup"),QDateTime(),follow);continue;}
        if(m_repository->updateReminderStatus(r.id,QStringLiteral("delivered"))){if(!explicitTask)s.setValue(QStringLiteral("proactive/lastDeliveredAt"),now.toString(Qt::ISODateWithMs));emit notificationRequested(QStringLiteral("Stellacandie 的提醒"),r.payload);if(cognitive.id>0){if(cognitive.recordType==QStringLiteral("event")){QDateTime follow(cognitive.scheduledAt.date().addDays(1),QTime(10,0));if(follow<now)follow=now.addSecs(60);m_repository->updateCognitiveRecord(cognitive.id,QStringLiteral("awaiting_followup"),QDateTime(),follow);}else m_repository->updateCognitiveRecord(cognitive.id,QStringLiteral("completed"));}break;}}
    const auto followups=m_repository->loadDueCognitiveFollowUps(now,5);for(const auto&event:followups){if(m_dnd||isQuietTime(now))break;emit notificationRequested(QStringLiteral("想起一件事"),QStringLiteral("之前的“%1”后来怎么样了？不想聊也没关系，我只问这一次。").arg(event.subject));m_repository->updateCognitiveRecord(event.id,QStringLiteral("archived"),QDateTime(),QDateTime(),event.followUpCount+1);break;}
}

void ProactiveBehaviorModule::seedLifestyleNudge()
{
    if(!m_repository||!m_enabled)return;QSettings s;const QString today=QDate::currentDate().toString(Qt::ISODate);if(s.value(QStringLiteral("proactive/lifestyleSeedDate")).toString()==today)return;QDateTime at(QDate::currentDate(),QTime(20,30));if(at<QDateTime::currentDateTime())at=at.addDays(1);if(schedule(QStringLiteral("gentle_rest"),at,QStringLiteral("如果还坐在电脑前，就起来伸个懒腰、喝两口水吧。只是小提醒，不想做也没关系。")))s.setValue(QStringLiteral("proactive/lifestyleSeedDate"),today);
}
