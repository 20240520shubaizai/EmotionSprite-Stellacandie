#include "src/logic/CognitiveRoutingRole.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QDateTime now(QDate(2026, 8, 31), QTime(16, 31));
    bool hasDate = false;
    bool hasClock = false;
    const QDateTime parsed = CognitiveRoutingRole::parseTime(
        QStringLiteral("请在两分钟后提醒我检查R2结果"), now, &hasDate, &hasClock);
    if (parsed != now.addSecs(120) || !hasDate || !hasClock)
        return 10;
    const auto relativeRoute = CognitiveRoutingRole::route(
        QStringLiteral("请在两分钟后提醒我检查R2结果"), now);
    if (!relativeRoute.explicitReminder || relativeRoute.needsTimeConfirmation
        || relativeRoute.subject != QStringLiteral("检查R2结果"))
        return 11;
    const auto absoluteRoute = CognitiveRoutingRole::route(
        QStringLiteral("明天晚上七点一定要提醒我换枕头"), now);
    if (!absoluteRoute.explicitReminder || absoluteRoute.needsTimeConfirmation)
        return 20;
    if (absoluteRoute.scheduledAt != QDateTime(QDate(2026, 9, 1), QTime(19, 0)))
        return 21;
    if (absoluteRoute.subject != QStringLiteral("换枕头")) {
        QTextStream(stderr) << "subject=" << absoluteRoute.subject << Qt::endl;
        return 22;
    }
    return 0;
}
