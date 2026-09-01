#include "AiCredentialStore.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <wincred.h>
#endif

namespace {
constexpr wchar_t CredentialTarget[] = L"EmotionSprite/DeepSeekApiKey";
constexpr wchar_t VisionCredentialTarget[] = L"EmotionSprite/SiliconFlowVisionApiKey";
constexpr wchar_t SyncCredentialTarget[] = L"EmotionSprite/CloudSyncSessionToken";

#ifdef Q_OS_WIN
bool saveCredential(const wchar_t*target,const QString&value){if(value.isEmpty()){if(CredDeleteW(target,CRED_TYPE_GENERIC,0))return true;return GetLastError()==ERROR_NOT_FOUND;}const QByteArray utf8=value.toUtf8();CREDENTIALW c{};c.Type=CRED_TYPE_GENERIC;c.TargetName=const_cast<LPWSTR>(target);c.CredentialBlobSize=static_cast<DWORD>(utf8.size());c.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(utf8.constData()));c.Persist=CRED_PERSIST_LOCAL_MACHINE;c.UserName=const_cast<LPWSTR>(L"EmotionSprite");return CredWriteW(&c,0)==TRUE;}
QString loadCredential(const wchar_t*target){PCREDENTIALW c=nullptr;if(!CredReadW(target,CRED_TYPE_GENERIC,0,&c))return{};const QByteArray utf8(reinterpret_cast<const char*>(c->CredentialBlob),static_cast<int>(c->CredentialBlobSize));const QString result=QString::fromUtf8(utf8);CredFree(c);return result;}
#endif
}

bool AiCredentialStore::saveApiKey(const QString &apiKey)
{
#ifdef Q_OS_WIN
    if (apiKey.isEmpty()) {
        return clearApiKey();
    }
    const QByteArray utf8 = apiKey.toUtf8();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(CredentialTarget);
    credential.CredentialBlobSize = static_cast<DWORD>(utf8.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(utf8.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"EmotionSprite");
    return CredWriteW(&credential, 0) == TRUE;
#else
    Q_UNUSED(apiKey)
    return false;
#endif
}

QString AiCredentialStore::loadApiKey()
{
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(CredentialTarget, CRED_TYPE_GENERIC, 0, &credential)) {
        return {};
    }
    const QByteArray utf8(reinterpret_cast<const char *>(credential->CredentialBlob),
                          static_cast<int>(credential->CredentialBlobSize));
    const QString result = QString::fromUtf8(utf8);
    CredFree(credential);
    return result;
#else
    return {};
#endif
}

bool AiCredentialStore::clearApiKey()
{
#ifdef Q_OS_WIN
    if (CredDeleteW(CredentialTarget, CRED_TYPE_GENERIC, 0)) {
        return true;
    }
    return GetLastError() == ERROR_NOT_FOUND;
#else
    return false;
#endif
}

bool AiCredentialStore::saveVisionApiKey(const QString&key){
#ifdef Q_OS_WIN
    return saveCredential(VisionCredentialTarget,key);
#else
    Q_UNUSED(key) return false;
#endif
}
QString AiCredentialStore::loadVisionApiKey(){
#ifdef Q_OS_WIN
    return loadCredential(VisionCredentialTarget);
#else
    return {};
#endif
}
bool AiCredentialStore::clearVisionApiKey(){return saveVisionApiKey(QString());}
bool AiCredentialStore::saveSyncToken(const QString&token){
#ifdef Q_OS_WIN
    return saveCredential(SyncCredentialTarget,token);
#else
    Q_UNUSED(token) return false;
#endif
}
QString AiCredentialStore::loadSyncToken(){
#ifdef Q_OS_WIN
    return loadCredential(SyncCredentialTarget);
#else
    return {};
#endif
}
bool AiCredentialStore::clearSyncToken(){return saveSyncToken(QString());}
