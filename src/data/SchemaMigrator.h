#pragma once

#include <QSqlDatabase>
#include <QString>

class SchemaMigrator
{
public:
    static constexpr int LatestVersion = 24;
    static bool migrate(QSqlDatabase database, QString *error = nullptr);
    static int currentVersion(QSqlDatabase database);
};
