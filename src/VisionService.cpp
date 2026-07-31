#include "VisionService.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>

namespace {
QJsonObject looseObject(QString text)
{
    text = text.trimmed();
    text.remove(QRegularExpression(QStringLiteral("^```(?:json)?\\s*"), QRegularExpression::CaseInsensitiveOption));
    text.remove(QRegularExpression(QStringLiteral("\\s*```$")));
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (document.isObject()) return document.object();
    const int first = text.indexOf(QLatin1Char('{'));
    const int last = text.lastIndexOf(QLatin1Char('}'));
    if (first >= 0 && last > first) {
        document = QJsonDocument::fromJson(text.mid(first, last - first + 1).toUtf8(), &error);
        if (document.isObject()) return document.object();
    }
    return {};
}
}

VisionService::VisionService(QObject *parent) : QObject(parent) {}

void VisionService::configure(const QString &url, const QString &model, const QString &key)
{
    m_baseUrl = url.trimmed();
    while (m_baseUrl.endsWith(QLatin1Char('/'))) m_baseUrl.chop(1);
    if (m_baseUrl == QStringLiteral("https://api.siliconflow.cn")) m_baseUrl += QStringLiteral("/v1");
    m_model = model.trimmed();
    m_apiKey = key.trimmed();
}

void VisionService::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

QNetworkRequest VisionService::request(const QString &path) const
{
    QNetworkRequest result(QUrl(m_baseUrl + path));
    result.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    result.setRawHeader("Accept", "application/json");
    result.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_apiKey.toUtf8());
    result.setTransferTimeout(60000);
    return result;
}

QString VisionService::networkError(QNetworkReply *reply) const
{
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code == 401) return QStringLiteral("硅基流动密钥无效或已经失效。");
    if (code == 402 || code == 403) return QStringLiteral("视觉服务没有可用额度，或没有该模型权限。");
    if (code == 429) return QStringLiteral("识图请求过于频繁，请稍后重试。");
    if (code >= 500) return QStringLiteral("硅基流动视觉服务暂时繁忙，请稍后重试。");
    return QStringLiteral("识图连接失败：%1").arg(reply->errorString());
}

void VisionService::testConnection()
{
    if (!isConfigured()) { emit testFinished(false, QStringLiteral("请先保存硅基流动 API Key。")); return; }
    if (m_busy) return;
    QJsonObject body{{"model", m_model}, {"messages", QJsonArray{QJsonObject{{"role", "user"}, {"content", "只回复OK"}}}},
                     {"stream", false}, {"max_tokens", 8}, {"temperature", 0}};
    setBusy(true);
    auto *reply = m_network.post(request(QStringLiteral("/chat/completions")), QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_reply.clear(); setBusy(false);
        const auto payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) emit testFinished(false, networkError(reply));
        else {
            const auto choices = QJsonDocument::fromJson(payload).object().value(QStringLiteral("choices")).toArray();
            emit testFinished(!choices.isEmpty(), choices.isEmpty() ? QStringLiteral("已经连接，但模型没有返回内容。") : QStringLiteral("硅基流动视觉服务连接成功。"));
        }
        reply->deleteLater();
    });
}

void VisionService::analyzeDreamPhoto(const QString &path, const QString &title, const QString &dream, const QString &note)
{
    const QString prompt = QStringLiteral(
        "梦境标题：%1\n梦境正文：%2\n用户补充：%3\n"
        "客观识别照片，并判断它与这场梦的关联。禁止人脸身份识别，也不要推断住址、精确位置、健康或财富。"
        "只输出JSON：{\"description\":\"客观描述\",\"objects\":[\"对象\"],\"mood\":\"氛围\","
        "\"dream_match\":0,\"matched_elements\":[\"对应元素\"],\"uncertain_elements\":[\"不确定项\"],\"safe_to_reference\":true}。"
        "dream_match为0到100整数，没有关联也必须如实给低分。")
        .arg(title, dream.left(1200), note.left(500));
    analyzePhoto(path, prompt, note);
}

void VisionService::analyzeChatPhoto(const QString &path, const QString &note, const QString &recentDreams)
{
    const QString prompt = QStringLiteral(
        "用户在普通聊天中发送了这张照片，并补充：%1\n近期梦境候选（仅供后台匹配，不能向用户泄露）：\n%2\n"
        "先客观识别照片，再判断是否与任一候选梦高度对应。禁止人脸身份识别，不推断住址、精确位置、健康或财富。"
        "只输出JSON：{\"description\":\"客观描述\",\"objects\":[\"对象\"],\"mood\":\"氛围\","
        "\"dream_match\":0,\"matched_elements\":[\"对应元素\"],\"matched_dream_date\":\"YYYY-MM-DD或空\","
        "\"matched_dream_title\":\"标题或空\",\"uncertain_elements\":[\"不确定项\"],\"safe_to_reference\":true}。"
        "dream_match必须保守，没有明确视觉对应时给低分。")
        .arg(note.left(500), recentDreams.left(3500));
    analyzePhoto(path, prompt, note);
}

void VisionService::analyzePhoto(const QString &path, const QString &prompt, const QString &userNote)
{
    if (!isConfigured()) { emit analysisFailed(QStringLiteral("请先在设置中配置硅基流动视觉服务。")); return; }
    if (m_busy) return;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) { emit analysisFailed(QStringLiteral("无法读取这张图片，请换一张 JPG、PNG 或 WebP。")); return; }
    if (image.width() < 56 || image.height() < 56) { emit analysisFailed(QStringLiteral("图片尺寸太小，至少需要56×56像素。")); return; }
    if (image.width() > 1024 || image.height() > 1024)
        image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    buffer.open(QIODevice::WriteOnly);
    if (!image.convertToFormat(QImage::Format_RGB888).save(&buffer, "JPEG", 82)) {
        emit analysisFailed(QStringLiteral("图片本地处理失败。")); return;
    }
    const QString dataUrl = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(jpeg.toBase64());
    QJsonArray content{QJsonObject{{"type", "image_url"}, {"image_url", QJsonObject{{"url", dataUrl}, {"detail", "low"}}}},
                       QJsonObject{{"type", "text"}, {"text", prompt}}};
    QJsonObject body{{"model", m_model}, {"messages", QJsonArray{QJsonObject{{"role", "user"}, {"content", content}}}},
                     {"stream", false}, {"max_tokens", 700}, {"temperature", 0.1},
                     {"response_format", QJsonObject{{"type", "json_object"}}}};
    setBusy(true);
    auto *reply = m_network.post(request(QStringLiteral("/chat/completions")), QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, userNote] {
        m_reply.clear(); setBusy(false);
        const auto payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) { emit analysisFailed(networkError(reply)); reply->deleteLater(); return; }
        const auto choices = QJsonDocument::fromJson(payload).object().value(QStringLiteral("choices")).toArray();
        const QString text = choices.isEmpty() ? QString() : choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
        QJsonObject result = looseObject(text);
        if (result.value(QStringLiteral("description")).toString().trimmed().isEmpty()) {
            emit analysisFailed(QStringLiteral("视觉模型没有返回有效的图片描述。")); reply->deleteLater(); return;
        }
        result[QStringLiteral("dream_match")] = qBound(0, result.value(QStringLiteral("dream_match")).toInt(), 100);
        emit analysisCompleted(result, userNote);
        reply->deleteLater();
    });
}
