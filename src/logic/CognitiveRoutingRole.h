#pragma once

#include <QDateTime>
#include <QString>

struct CognitiveRouteResult {
    QString action;
    QString recordType;
    QString subject;
    QDateTime scheduledAt;
    bool explicitReminder=false;
    bool needsTimeConfirmation=false;
    int deliveryPriority=50;
    int memoryImportance=25;
    QString followUpPolicy=QStringLiteral("none");
    int maxFollowUps=0;
};

class CognitiveRoutingRole
{
public:
    static CognitiveRouteResult route(const QString &message,const QDateTime &now=QDateTime::currentDateTime());
    static QDateTime parseTime(const QString &message,const QDateTime &now,bool *hasDate=nullptr,bool *hasClock=nullptr);
    static QString normalizedSubject(const QString &message);
};
