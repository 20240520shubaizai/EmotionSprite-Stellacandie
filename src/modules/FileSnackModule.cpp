#include "FileSnackModule.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>

namespace {
QString normalizedDirectory(const QString &path)
{
    QString result = QDir::fromNativeSeparators(QDir::cleanPath(path)).toLower();
    if (!result.endsWith(QLatin1Char('/'))) result += QLatin1Char('/');
    return result;
}
}

FileSnackModule::FileSnackModule(DataRepository *storage, QObject *parent)
    : FeatureModule(parent), m_storage(storage)
{
    m_enabled = QSettings().value(QStringLiteral("modules/fileSnackEnabled"), true).toBool();
    refreshData();
}

QString FileSnackModule::id() const { return QStringLiteral("file_snack"); }
QString FileSnackModule::displayName() const { return QStringLiteral("文件零食"); }
bool FileSnackModule::isEnabled() const { return m_enabled; }

void FileSnackModule::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("modules/fileSnackEnabled"), enabled);
    emit enabledChanged(enabled);
    emit changed();
}

bool FileSnackModule::prepare(const QUrl &url)
{
    clear();
    if (!m_enabled) { m_refusalReason = QStringLiteral("零食工厂目前没有开启。"); emit changed(); return false; }
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) m_refusalReason = QStringLiteral("请选择一个确实存在的普通文件。");
    else if (file.isSymLink() || file.suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0) m_refusalReason = QStringLiteral("为了避免误删目标文件，快捷方式不能加工。");
    else if (!file.isWritable()) m_refusalReason = QStringLiteral("这个文件不可写，可能正在被占用或需要管理员权限。");
    if (!m_refusalReason.isEmpty()) { emit changed(); return false; }

    const QString canonical = QDir::fromNativeSeparators(QDir::cleanPath(file.canonicalFilePath())).toLower();
    QStringList protectedPaths{
        normalizedDirectory(QCoreApplication::applicationDirPath()),
        normalizedDirectory(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
        normalizedDirectory(qEnvironmentVariable("WINDIR"))
    };
    protectedPaths.append(QSettings().value(QStringLiteral("fileSnack/protectedDirectories")).toStringList());
    for (const QString &protectedPath : std::as_const(protectedPaths)) {
        if (!protectedPath.isEmpty() && canonical.startsWith(normalizedDirectory(protectedPath))) {
            m_refusalReason = QStringLiteral("这个文件位于受保护目录中，零食工厂拒绝加工。");
            emit changed(); return false;
        }
    }

    m_sourcePath = file.absoluteFilePath();
    m_fileName = file.fileName();
    m_sourceSize = file.size();
    m_fileInfo = QLocale().formattedDataSize(file.size());
    m_modifiedText = file.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    const QString ext = file.suffix().toLower();
    if (QStringList{"png","jpg","jpeg","gif","webp","bmp"}.contains(ext)) { m_snackType="image"; m_snackName=QStringLiteral("彩虹果冻"); m_snackEmoji=QStringLiteral("🍮"); }
    else if (QStringList{"txt","md","doc","docx","pdf"}.contains(ext)) { m_snackType="document"; m_snackName=QStringLiteral("纸页薄脆"); m_snackEmoji=QStringLiteral("🍘"); }
    else if (QStringList{"mp3","wav","flac","ogg"}.contains(ext)) { m_snackType="audio"; m_snackName=QStringLiteral("音符糖"); m_snackEmoji=QStringLiteral("🍬"); }
    else if (QStringList{"mp4","mkv","avi","mov"}.contains(ext)) { m_snackType="video"; m_snackName=QStringLiteral("电影爆米花"); m_snackEmoji=QStringLiteral("🍿"); }
    else if (QStringList{"zip","7z","rar","tar","gz"}.contains(ext)) { m_snackType="archive"; m_snackName=QStringLiteral("压缩夹心饼"); m_snackEmoji=QStringLiteral("🍪"); }
    else if (QStringList{"cpp","c","h","hpp","qml","py","js","json"}.contains(ext)) { m_snackType="code"; m_snackName=QStringLiteral("代码脆片"); m_snackEmoji=QStringLiteral("🥨"); }
    else { m_snackType="other"; m_snackName=QStringLiteral("神秘小方糖"); m_snackEmoji=QStringLiteral("🧊"); }
    m_nutrition = std::clamp<int>(2 + static_cast<int>(file.size() / 1024 / 1024), 2, 8);

    const bool isRecent = file.lastModified().secsTo(QDateTime::currentDateTime()) < 24 * 60 * 60;
    const bool isLarge = file.size() > 500LL * 1024 * 1024;
    const bool isExecutable = QStringList{"exe","msi","bat","cmd","com","dll","sys","ps1","scr"}.contains(ext);
    m_strongConfirmationRequired = isRecent || isLarge || isExecutable;
    m_safetyLevel = m_strongConfirmationRequired ? QStringLiteral("需要特别确认") : QStringLiteral("普通确认");
    QStringList reasons;
    if (isRecent) reasons << QStringLiteral("文件在24小时内修改过");
    if (isLarge) reasons << QStringLiteral("文件大于500 MB");
    if (isExecutable) reasons << QStringLiteral("这是可执行或脚本文件");
    m_warningText = reasons.isEmpty()
        ? QStringLiteral("确认后原文件会移入 Windows 回收站，可从回收站恢复。")
        : QStringLiteral("%1。请确认它确实不再需要；原文件只会移入回收站。").arg(reasons.join(QStringLiteral("；")));
    m_sourcePaths={m_sourcePath};m_sourceNames={m_fileName};m_sourceTypes={m_snackType};m_sourceSizes={m_sourceSize};
    emit changed(); return true;
}

bool FileSnackModule::prepareMany(const QList<QUrl> &urls)
{
    if(urls.isEmpty()){clear();m_refusalReason=QStringLiteral("没有选择文件。");emit changed();return false;}
    if(!prepare(urls.first()))return false;
    bool anyStrong=m_strongConfirmationRequired;
    for(int i=1;i<urls.size();++i){
        const QString path=urls.at(i).isLocalFile()?urls.at(i).toLocalFile():urls.at(i).toString();const QFileInfo f(path);
        if(!f.exists()||!f.isFile()||f.isSymLink()||!f.isWritable()||f.suffix().compare("lnk",Qt::CaseInsensitive)==0){const QString reason=QStringLiteral("批次中包含无法安全加工的文件：%1").arg(f.fileName());clear();m_refusalReason=reason;emit changed();return false;}
        const QString canonical=QDir::fromNativeSeparators(QDir::cleanPath(f.canonicalFilePath())).toLower();QStringList protectedPaths{normalizedDirectory(QCoreApplication::applicationDirPath()),normalizedDirectory(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),normalizedDirectory(qEnvironmentVariable("WINDIR"))};protectedPaths.append(QSettings().value(QStringLiteral("fileSnack/protectedDirectories")).toStringList());
        for(const QString&p:std::as_const(protectedPaths))if(!p.isEmpty()&&canonical.startsWith(normalizedDirectory(p))){const QString reason=QStringLiteral("批次中包含受保护目录文件：%1").arg(f.fileName());clear();m_refusalReason=reason;emit changed();return false;}
        const QString ext=f.suffix().toLower();QString type="other";
        if(QStringList{"png","jpg","jpeg","gif","webp","bmp"}.contains(ext))type="image";else if(QStringList{"txt","md","doc","docx","pdf"}.contains(ext))type="document";else if(QStringList{"mp3","wav","flac","ogg"}.contains(ext))type="audio";else if(QStringList{"mp4","mkv","avi","mov"}.contains(ext))type="video";else if(QStringList{"zip","7z","rar","tar","gz"}.contains(ext))type="archive";else if(QStringList{"cpp","c","h","hpp","qml","py","js","json"}.contains(ext))type="code";
        m_sourcePaths<<f.absoluteFilePath();m_sourceNames<<f.fileName();m_sourceTypes<<type;m_sourceSizes<<f.size();m_sourceSize+=f.size();
        anyStrong=anyStrong||f.lastModified().secsTo(QDateTime::currentDateTime())<86400||f.size()>500LL*1024*1024||QStringList{"exe","msi","bat","cmd","com","dll","sys","ps1","scr"}.contains(ext);
    }
    if(m_sourcePaths.size()>1){
        QStringList unique=m_sourceTypes;unique.removeDuplicates();std::sort(unique.begin(),unique.end());m_snackType=QStringLiteral("fusion_")+unique.join('_');m_snackEmoji=QStringLiteral("🎁");m_snackName=QStringLiteral("混合零食礼包");
        if(unique.contains("image")&&unique.contains("audio")&&unique.contains("code")){m_snackType="hidden_dream_palette";m_snackName=QStringLiteral("隐藏款·梦境调色盘");m_snackEmoji=QStringLiteral("🌌");}
        else if(unique.size()>=4){m_snackType="hidden_chaos_galaxy";m_snackName=QStringLiteral("隐藏款·混沌银河盒");m_snackEmoji=QStringLiteral("🪐");}
        else if(unique.contains("image")&&unique.contains("document")){m_snackName=QStringLiteral("故事相册芭菲");m_snackEmoji=QStringLiteral("🍨");}
        else if(unique.contains("audio")&&unique.contains("video")){m_snackName=QStringLiteral("星光影院圣代");m_snackEmoji=QStringLiteral("🎞️");}
        else if(unique.contains("code")&&unique.contains("document")){m_snackName=QStringLiteral("知识齿轮酥");m_snackEmoji=QStringLiteral("⚙️");}
        m_fileName=QStringLiteral("%1 个文件").arg(m_sourcePaths.size());m_fileInfo=QLocale().formattedDataSize(m_sourceSize);m_modifiedText=QStringLiteral("批量加工");m_nutrition=std::clamp(3+static_cast<int>(m_sourcePaths.size()),3,8);
        m_warningText=QStringLiteral("将一次处理 %1 个文件。请逐项核对列表；确认后它们会分别移入回收站。").arg(m_sourcePaths.size());
    }
    m_strongConfirmationRequired=anyStrong;m_safetyLevel=anyStrong?QStringLiteral("需要特别确认"):QStringLiteral("普通确认");emit changed();return true;
}

bool FileSnackModule::movePendingToTrash(QString *errorMessage)
{
    if (!hasPendingSnack()) { if (errorMessage) *errorMessage=QStringLiteral("还没有选择零食文件。"); return false; }
    for(const QString &path:std::as_const(m_sourcePaths)){QString trashedPath;if(!QFile::moveToTrash(path,&trashedPath)){if(errorMessage)*errorMessage=QStringLiteral("批量处理在 %1 处停止。已处理文件仍可从回收站恢复。").arg(QFileInfo(path).fileName());return false;}}
    return true;
}

bool FileSnackModule::storePending(QString *errorMessage)
{
    if (!movePendingToTrash(errorMessage)) return false;
    if (!m_storage || !m_storage->addSnackToInventory(m_snackType,m_snackName,m_snackEmoji,m_nutrition)) {
        if (errorMessage) *errorMessage=QStringLiteral("文件已进入回收站，但零食袋记录失败；原文件仍可恢复。");
        clear(); return false;
    }
    m_storage->addSnackHistory(QStringLiteral("stored"),m_snackType,m_snackName,m_sourceNames.join(QStringLiteral("、")),m_sourceSize,m_safetyLevel);
    clear(); refreshData(); emit changed(); return true;
}

QString FileSnackModule::reactionFor(const SnackCatalogRecord &record) const
{
    if (record.consecutiveCount >= 3) return QStringLiteral("这个最近吃得有点多啦，下一次换个口味给我嘛。");
    if (record.preference >= 80) return QStringLiteral("哇，是我很喜欢的味道！再给我一口——");
    if (record.preference >= 65) return QStringLiteral("咔嚓咔嚓，这个很好吃，我记住啦！");
    if (record.preference >= 50) return QStringLiteral("味道有点奇妙，不过我愿意再研究一下。");
    return QStringLiteral("唔……这个口味我不太擅长，但还是谢谢你喂我。");
}

bool FileSnackModule::consumePending(QString *errorMessage)
{
    if (!movePendingToTrash(errorMessage)) return false;
    const QString type=m_snackType,name=m_snackName,emoji=m_snackEmoji,source=m_sourceNames.join(QStringLiteral("、")),safety=m_safetyLevel;
    const qint64 size=m_sourceSize; const int nutrition=m_nutrition;
    const SnackCatalogRecord record=m_storage?m_storage->recordSnackEaten(type,name,emoji):SnackCatalogRecord{};
    if (m_storage) m_storage->addSnackHistory(QStringLiteral("eaten"),type,name,source,size,safety);
    const QString reaction=reactionFor(record); clear(); refreshData();
    emit snackConsumed(name,emoji,nutrition,reaction); emit changed(); return true;
}

bool FileSnackModule::eatInventory(int row, QString *errorMessage)
{
    if (row<0 || row>=m_inventory.size()) { if(errorMessage)*errorMessage=QStringLiteral("这份零食已经不在袋子里了。"); return false; }
    const SnackInventoryRecord item=m_inventory.at(row);
    if (!m_storage || !m_storage->consumeSnackInventory(item.id)) { if(errorMessage)*errorMessage=QStringLiteral("取出零食失败，请刷新后重试。"); return false; }
    const SnackCatalogRecord record=m_storage->recordSnackEaten(item.snackType,item.snackName,item.emoji);
    m_storage->addSnackHistory(QStringLiteral("eaten_from_bag"),item.snackType,item.snackName,QString(),0,QStringLiteral("已加工"));
    const QString reaction=reactionFor(record); refreshData();
    emit snackConsumed(item.snackName,item.emoji,item.nutrition,reaction); emit changed(); return true;
}

bool FileSnackModule::protectPendingDirectory(QString *errorMessage)
{
    if (!hasPendingSnack()) { if(errorMessage)*errorMessage=QStringLiteral("请先选择该目录中的一个文件。"); return false; }
    QStringList paths=QSettings().value(QStringLiteral("fileSnack/protectedDirectories")).toStringList();
    const QString directory=normalizedDirectory(QFileInfo(m_sourcePath).absolutePath());
    if (!paths.contains(directory,Qt::CaseInsensitive)) { paths << directory; QSettings().setValue(QStringLiteral("fileSnack/protectedDirectories"),paths); }
    clear(); emit changed(); return true;
}

QStringList FileSnackModule::inventoryItems() const
{
    QStringList out; for(const auto &item:m_inventory) out << QStringLiteral("%1  %2  ×%3|%4").arg(item.emoji,item.snackName).arg(item.quantity).arg(item.id); return out;
}
QStringList FileSnackModule::catalogItems() const
{
    QStringList out; for(const auto &item:m_catalog) out << QStringLiteral("%1  %2｜吃过 %3 次｜喜爱度 %4").arg(item.emoji,item.snackName).arg(item.eatenCount).arg(item.preference); return out;
}
QStringList FileSnackModule::historyItems() const { return m_history; }
QStringList FileSnackModule::protectedDirectories() const { return QSettings().value(QStringLiteral("fileSnack/protectedDirectories")).toStringList(); }
QStringList FileSnackModule::pendingFileItems() const{QStringList out;for(int i=0;i<m_sourceNames.size();++i)out<<QStringLiteral("%1 · %2").arg(m_sourceNames.at(i),QLocale().formattedDataSize(m_sourceSizes.value(i)));return out;}
int FileSnackModule::inventoryNutrition(int row)const{return row>=0&&row<m_inventory.size()?m_inventory.at(row).nutrition:0;}
qint64 FileSnackModule::inventoryId(int row)const{return row>=0&&row<m_inventory.size()?m_inventory.at(row).id:0;}
int FileSnackModule::rowForInventoryId(qint64 id)const{for(int i=0;i<m_inventory.size();++i)if(m_inventory.at(i).id==id)return i;return -1;}

void FileSnackModule::refreshData()
{
    if (!m_storage) return;
    const auto inventory=m_storage->loadSnackInventory(); const auto catalog=m_storage->loadSnackCatalog(); const auto history=m_storage->loadSnackHistory(40);
    m_inventory.clear();m_inventory.reserve(inventory.size());for(const auto &item:inventory)m_inventory.append(item);
    m_catalog.clear();m_catalog.reserve(catalog.size());for(const auto &item:catalog)m_catalog.append(item);
    m_history.clear();m_history.reserve(history.size());for(const auto &item:history)m_history.append(item);
}

void FileSnackModule::clear()
{
    m_sourcePath.clear(); m_fileName.clear(); m_fileInfo.clear(); m_modifiedText.clear(); m_snackType.clear();
    m_snackName.clear(); m_snackEmoji.clear(); m_warningText.clear(); m_safetyLevel.clear(); m_refusalReason.clear();
    m_sourcePaths.clear();m_sourceNames.clear();m_sourceTypes.clear();m_sourceSizes.clear();
    m_sourceSize=0; m_nutrition=0; m_strongConfirmationRequired=false; emit changed();
}
