#include "CognitiveRoutingRole.h"

#include <QRegularExpression>

QDateTime CognitiveRoutingRole::parseTime(const QString &text,const QDateTime &now,bool *hasDate,bool *hasClock)
{
    QString normalized=text;const QList<QPair<QString,QString>> numerals{{QStringLiteral("十二"),QStringLiteral("12")},{QStringLiteral("十一"),QStringLiteral("11")},{QStringLiteral("十"),QStringLiteral("10")},{QStringLiteral("九"),QStringLiteral("9")},{QStringLiteral("八"),QStringLiteral("8")},{QStringLiteral("七"),QStringLiteral("7")},{QStringLiteral("六"),QStringLiteral("6")},{QStringLiteral("五"),QStringLiteral("5")},{QStringLiteral("四"),QStringLiteral("4")},{QStringLiteral("三"),QStringLiteral("3")},{QStringLiteral("二"),QStringLiteral("2")},{QStringLiteral("两"),QStringLiteral("2")},{QStringLiteral("一"),QStringLiteral("1")}};for(const auto&p:numerals)normalized.replace(p.first,p.second);
    bool dateFound=false,clockFound=false;QDate date=now.date();QTime time;
    if(text.contains(QStringLiteral("后天"))){date=date.addDays(2);dateFound=true;}
    else if(text.contains(QStringLiteral("明天"))){date=date.addDays(1);dateFound=true;}
    else if(text.contains(QStringLiteral("今天"))||text.contains(QStringLiteral("今晚"))){dateFound=true;}
    QRegularExpression daysRe(QStringLiteral("(\\d{1,3})\\s*天后"));auto days=daysRe.match(normalized);if(days.hasMatch()){date=now.date().addDays(days.captured(1).toInt());dateFound=true;}
    QRegularExpression dateRe(QStringLiteral("(\\d{1,2})月(\\d{1,2})(?:日|号)?"));auto dm=dateRe.match(text);if(dm.hasMatch()){int year=now.date().year();date=QDate(year,dm.captured(1).toInt(),dm.captured(2).toInt());if(date<now.date())date=date.addYears(1);dateFound=date.isValid();}
    QRegularExpression clockRe(QStringLiteral("(上午|中午|下午|晚上|今晚|凌晨)?\\s*(\\d{1,2})(?:点|时)(?:(\\d{1,2})分)?|(?:上午|中午|下午|晚上|今晚|凌晨)?\\s*(\\d{1,2})[:：](\\d{1,2})"));auto cm=clockRe.match(normalized);
    if(cm.hasMatch()){const bool colonForm=!cm.captured(4).isEmpty();int hour=(colonForm?cm.captured(4):cm.captured(2)).toInt(),minute=(colonForm?cm.captured(5):cm.captured(3)).toInt();const QString period=cm.captured(1);if((period==QStringLiteral("下午")||period==QStringLiteral("晚上")||period==QStringLiteral("今晚"))&&hour<12)hour+=12;if(period==QStringLiteral("中午")&&hour<11)hour+=12;if(period==QStringLiteral("凌晨")&&hour==12)hour=0;time=QTime(hour,minute);clockFound=time.isValid();}
    QRegularExpression minutesRe(QStringLiteral("(\\d{1,4})\\s*分钟后"));auto mm=minutesRe.match(normalized);if(mm.hasMatch()){if(hasDate)*hasDate=true;if(hasClock)*hasClock=true;return now.addSecs(mm.captured(1).toInt()*60);}
    QRegularExpression hoursRe(QStringLiteral("(\\d{1,3})\\s*小时后"));auto hm=hoursRe.match(normalized);if(hm.hasMatch()){if(hasDate)*hasDate=true;if(hasClock)*hasClock=true;return now.addSecs(hm.captured(1).toInt()*3600);}
    if(hasDate)*hasDate=dateFound;if(hasClock)*hasClock=clockFound;
    if(dateFound&&clockFound)return QDateTime(date,time);
    if(dateFound)return QDateTime(date,QTime(0,0));
    if(clockFound)return QDateTime(now.date(),time);
    return {};
}

QString CognitiveRoutingRole::normalizedSubject(const QString &message)
{
    QString s=message;
    s.remove(QRegularExpression(QStringLiteral("(?:请|你)?(?:一定|千万|务必)?(?:记得)?(?:到时候)?(?:提醒|通知|叫)(?:一下)?我")));
    s.remove(QRegularExpression(QStringLiteral("(?:今天|今晚|明天|后天|大后天|\\d{1,3}天后|(?:\\d{1,4}|[一二两三四五六七八九十半])分钟后|(?:\\d{1,3}|[一二两三四五六七八九十半])小时后|\\d{1,2}月\\d{1,2}(?:日|号)?|(?:上午|中午|下午|晚上|凌晨)?\\s*(?:\\d{1,2}|[一二两三四五六七八九十]{1,3})(?:点|时|:|：)(?:(?:\\d{1,2}|[一二两三四五六七八九十]{1,3})分)?)")));
    s.remove(QRegularExpression(QStringLiteral("^(?:请|在|我|要|去|准备|计划|一定要|必须|务必)+")));
    s.remove(QRegularExpression(QStringLiteral("[，。！？!?；;]+$")));
    return s.trimmed().left(80);
}

CognitiveRouteResult CognitiveRoutingRole::route(const QString &message,const QDateTime &now)
{
    CognitiveRouteResult r;const QString text=message.trimmed();
    const bool asksReminder=QRegularExpression(QStringLiteral("(?:提醒我|通知我|叫我|别让我忘|不要让我忘|记得提醒|到时候提醒)" )).match(text).hasMatch();
    const bool eventWords=QRegularExpression(QStringLiteral("(?:开会|会议|面试|考试|看医生|去医院|聚会|约会|出差|答辩|汇报)" )).match(text).hasMatch();
    if(QRegularExpression(QStringLiteral("(?:取消|不用|不需要|别再).{0,8}(?:提醒|会议|约定|任务)" )).match(text).hasMatch()){r.action=QStringLiteral("cancel");r.subject=normalizedSubject(text);return r;}
    if(QRegularExpression(QStringLiteral("(?:已经|我)?(?:完成了|做完了|解决了|结束了)" )).match(text).hasMatch()){r.action=QStringLiteral("complete");r.subject=normalizedSubject(text);return r;}
    if(QRegularExpression(QStringLiteral("(?:改到|改成|推迟到|延期到|提前到)" )).match(text).hasMatch()){r.action=QStringLiteral("reschedule");r.subject=normalizedSubject(text);bool d=false,c=false;r.scheduledAt=parseTime(text,now,&d,&c);r.needsTimeConfirmation=!r.scheduledAt.isValid();return r;}
    if(!asksReminder&&!eventWords)return r;
    bool hasDate=false,hasClock=false;const QDateTime at=parseTime(text,now,&hasDate,&hasClock);
    r.action=QStringLiteral("create");r.explicitReminder=asksReminder;r.recordType=eventWords?QStringLiteral("event"):QStringLiteral("reminder");r.subject=normalizedSubject(text);if(r.subject.isEmpty())r.subject=eventWords?QStringLiteral("日程事件"):QStringLiteral("待提醒事项");
    r.scheduledAt=at;r.needsTimeConfirmation=asksReminder&&(!hasDate||!hasClock);r.deliveryPriority=asksReminder?100:65;r.memoryImportance=eventWords?25:20;
    if(eventWords){r.followUpPolicy=QStringLiteral("once_next_day");r.maxFollowUps=1;}
    return r;
}
