#pragma once

#include <QString>

class AiCredentialStore final
{
public:
    static bool saveApiKey(const QString &apiKey);
    static QString loadApiKey();
    static bool clearApiKey();
    static bool saveVisionApiKey(const QString &apiKey);
    static QString loadVisionApiKey();
    static bool clearVisionApiKey();
};
