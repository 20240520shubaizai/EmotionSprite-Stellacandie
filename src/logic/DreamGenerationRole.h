#pragma once

#include <QJsonObject>
#include <QString>

class DreamGenerationRole final
{
public:
    static QString chooseTheme();
    static bool normalizeAndValidate(const QJsonObject &input,QJsonObject *output,QString *error);
};
