#include "VisionRecognitionModule.h"
#include "../VisionService.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSettings>

VisionRecognitionModule::VisionRecognitionModule(VisionService *service, QObject *parent)
    : FeatureModule(parent), m_service(service)
{
    m_enabled = QSettings().value(QStringLiteral("modules/visionEnabled"), true).toBool();
    connect(service, &VisionService::analysisCompleted, this, [this](const QJsonObject &result, const QString &note) {
        m_resultSummary = QStringLiteral("识别到：%1").arg(result.value(QStringLiteral("description")).toString());
        m_status = m_chatMode ? QStringLiteral("照片已经看完，正在组织回复……") : QStringLiteral("照片已经看完，正在把现实回声交给精灵……");
        const QString name = fileName();
        if (m_chatMode) emit chatRecognized(result, note, name);
        else emit recognized(result, note);
        emit changed();
    });
    connect(service, &VisionService::analysisFailed, this, [this](const QString &error) { m_status = error; emit changed(); });
    connect(service, &VisionService::busyChanged, this, &VisionRecognitionModule::changed);
}

void VisionRecognitionModule::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("modules/visionEnabled"), enabled);
    emit enabledChanged(enabled); emit changed();
}

bool VisionRecognitionModule::preparePhoto(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QFileInfo file(path);
    const QStringList allowed{QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp")};
    if (!m_enabled) { m_status = QStringLiteral("识图模块尚未开启。"); emit changed(); return false; }
    if (!file.exists() || !file.isFile() || !allowed.contains(file.suffix().toLower())) {
        m_status = QStringLiteral("请选择 JPG、PNG 或 WebP 图片。"); emit changed(); return false;
    }
    if (file.size() > 10LL * 1024 * 1024) { m_status = QStringLiteral("图片超过10MB，请先压缩后再选择。"); emit changed(); return false; }
    QImageReader reader(path);
    if (!reader.canRead()) { m_status = QStringLiteral("图片无法读取或已经损坏。"); emit changed(); return false; }
    m_path = file.absoluteFilePath();
    m_status = QStringLiteral("图片只在本地预览，确认发送前不会上传。");
    m_resultSummary.clear(); m_chatMode = false; emit changed(); return true;
}

void VisionRecognitionModule::clear()
{
    m_path.clear(); m_status.clear(); m_resultSummary.clear(); m_chatMode = false; emit changed();
}

QString VisionRecognitionModule::fileName() const { return QFileInfo(m_path).fileName(); }

void VisionRecognitionModule::analyze(const QString &title, const QString &content, const QString &note)
{
    if (m_path.isEmpty()) { m_status = QStringLiteral("请先选择一张照片。"); emit changed(); return; }
    m_chatMode = false; m_status = QStringLiteral("正在移除照片元数据并请视觉模型辨认……"); emit changed();
    m_service->analyzeDreamPhoto(m_path, title, content, note);
}

void VisionRecognitionModule::analyzeChat(const QString &note, const QString &recentDreams)
{
    if (m_path.isEmpty()) { m_status = QStringLiteral("请先选择一张照片。"); emit changed(); return; }
    m_chatMode = true; m_status = QStringLiteral("正在移除照片元数据并认真看图……"); emit changed();
    m_service->analyzeChatPhoto(m_path, note, recentDreams);
}
