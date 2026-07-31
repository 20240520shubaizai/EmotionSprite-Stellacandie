#include "MemeCultureModule.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>
#include <QXmlStreamReader>

namespace {
QString learnedPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/learned_memes.json");
}
}

MemeCultureModule::MemeCultureModule(QObject *parent) : FeatureModule(parent)
{
    QSettings settings;
    m_enabled = settings.value("modules/memeCultureEnabled", true).toBool();
    m_pendingTerm = settings.value("memes/pendingTerm").toString();
    m_pendingWebSummary = settings.value("memes/pendingWebSummary").toString();
    QFile bundled(":/assets/memes/hot_memes_zh_CN.json");
    if (bundled.open(QIODevice::ReadOnly))
        m_entries = QJsonDocument::fromJson(bundled.readAll()).object().value("entries").toArray();
    loadLearnedMemes();
}

QString MemeCultureModule::id() const { return "meme_culture"; }
QString MemeCultureModule::displayName() const { return QStringLiteral("热梗文化"); }
bool MemeCultureModule::isEnabled() const { return m_enabled; }
void MemeCultureModule::setEnabled(bool enabled)
{
    m_enabled = enabled;
    QSettings().setValue("modules/memeCultureEnabled", enabled);
}

void MemeCultureModule::loadLearnedMemes()
{
    QFile file(learnedPath());
    if (!file.open(QIODevice::ReadOnly)) return;
    m_learnedEntries = QJsonDocument::fromJson(file.readAll()).object().value("entries").toArray();
    for (const auto &entry : m_learnedEntries) m_entries.append(entry);
}

void MemeCultureModule::persistLearnedMemes()
{
    QSaveFile file(learnedPath());
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"entries", m_learnedEntries}})
                   .toJson(QJsonDocument::Indented));
    file.commit();
}

bool MemeCultureModule::fresh(const QJsonObject &entry) const
{
    if (entry.value("source").toString() == "user" || entry.value("source").toString() == "web+user"
        || entry.value("source").toString() == "shared") return true;
    const QDate date = QDate::fromString(entry.value("first_seen").toString(), Qt::ISODate);
    return date.isValid() && date.daysTo(QDate::currentDate()) <= 183;
}

int MemeCultureModule::recentCount() const
{
    int count = 0;
    for (QChar c : QSettings().value("memes/recentUsage", "0000000000").toString()) count += c == '1';
    return count;
}

bool MemeCultureModule::containsMeme(const QString &text) const
{
    for (const auto &value : m_entries) {
        const auto entry = value.toObject();
        if (text.contains(entry.value("phrase").toString(), Qt::CaseInsensitive)) return true;
        for (const auto &alias : entry.value("aliases").toArray())
            if (text.contains(alias.toString(), Qt::CaseInsensitive)) return true;
    }
    return false;
}

void MemeCultureModule::recordAssistantReply(const QString &reply)
{
    QString history = QSettings().value("memes/recentUsage", "0000000000").toString();
    history.append(containsMeme(reply) ? '1' : '0');
    QSettings().setValue("memes/recentUsage", history.right(10));
}

QString MemeCultureModule::searchKeyword(const QString &message) const
{
    for (const auto &value : m_entries) {
        const auto entry = value.toObject();
        const QString phrase = entry.value("phrase").toString();
        if (message.contains(phrase, Qt::CaseInsensitive)) return phrase;
        for (const auto &alias : entry.value("aliases").toArray())
            if (message.contains(alias.toString(), Qt::CaseInsensitive)) return phrase;
    }
    const QRegularExpression re(QStringLiteral("([\\p{Han}A-Za-z0-9]{1,12})(?:是什么梗|什么梗)"));
    const auto hit = re.match(message);
    if (hit.hasMatch()) return hit.captured(1);
    if (message.contains(QStringLiteral("王者"))) return QStringLiteral("王者荣耀");
    return {};
}

QString MemeCultureModule::inferMemeTerm(const QString &message, const QString &previous) const
{
    const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("[“\"『「]?([^，。！？,.!?]{1,16})[”\"』」]?(?:是一个梗|是个梗|这个梗)")),
        QRegularExpression(QStringLiteral("(?:以后这就叫|我们把这个叫|以后叫)[“\"『「]?([^，。！？,.!?]{1,16})"))
    };
    for (const auto &pattern : patterns) {
        const auto match = pattern.match(message);
        if (match.hasMatch()) {
            QString term = match.captured(1).trimmed();
            term.remove(QRegularExpression(QStringLiteral("^(哎呀|原来|这|那|它|这个|那个)")));
            term.remove(QRegularExpression(QStringLiteral("[”\"』」]+$")));
            if (!term.isEmpty()) return term.right(16);
        }
    }
    const auto quoted = QRegularExpression(QStringLiteral("[“\"『「]([^”\"』」]{1,16})[”\"』」]")).match(previous);
    if (quoted.hasMatch()) return quoted.captured(1).trimmed();
    QString cleaned = previous.trimmed();
    cleaned.remove(QRegularExpression(QStringLiteral("[，。！？,.!?~～]+$")));
    return cleaned.size() <= 16 ? cleaned : QString();
}

void MemeCultureModule::searchMeme(const QString &keyword, std::function<void(QString)> done)
{
    QUrl url(QStringLiteral("https://www.bing.com/search"));
    QUrlQuery query;
    query.addQueryItem("format", "rss");
    query.addQueryItem("q", keyword + QStringLiteral(" 什么梗 近期用法"));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setTransferTimeout(5000);
    request.setRawHeader("User-Agent", "Mozilla/5.0");
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
        QStringList snippets;
        if (reply->error() == QNetworkReply::NoError) {
            QXmlStreamReader xml(reply->readAll());
            while (!xml.atEnd() && snippets.size() < 3) {
                xml.readNext();
                if (xml.isStartElement() && (xml.name() == QStringLiteral("title") || xml.name() == QStringLiteral("description"))) {
                    QString text = xml.readElementText().remove(QRegularExpression("<[^>]+>")).simplified();
                    if (!text.isEmpty() && !text.contains("Bing:")) snippets << text.left(180);
                }
            }
        }
        reply->deleteLater();
        done(snippets.join(QStringLiteral("；")));
    });
}

void MemeCultureModule::saveLearnedMeme(const QString &term, const QString &webSummary,
                                         const QString &userView, bool shared)
{
    for (int i = m_learnedEntries.size() - 1; i >= 0; --i) {
        if (m_learnedEntries[i].toObject().value("phrase").toString().compare(term, Qt::CaseInsensitive) == 0)
            m_learnedEntries.removeAt(i);
    }
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const auto entry = m_entries[i].toObject();
        if ((entry.value("source").toString().contains("user") || entry.value("source").toString() == "shared")
            && entry.value("phrase").toString().compare(term, Qt::CaseInsensitive) == 0) m_entries.removeAt(i);
    }
    QString meaning = webSummary.trimmed();
    if (meaning.isEmpty()) meaning = QStringLiteral("暂未找到可靠的公开解释。");
    QJsonObject entry{{"id", QStringLiteral("user_") + QUuid::createUuid().toString(QUuid::Id128)},
                      {"phrase", term}, {"aliases", QJsonArray()},
                      {"category", shared ? QStringLiteral("共同梗") : QStringLiteral("用户学习")},
                      {"first_seen", QDate::currentDate().toString(Qt::ISODate)}, {"meaning", meaning},
                      {"user_view", userView.trimmed()},
                      {"source", shared ? "shared" : (userView.trimmed().isEmpty() ? "user" : "web+user")},
                      {"usage", "both"},
                      {"style_hint", userView.trimmed().isEmpty()
                           ? QStringLiteral("按检索到的常见语境自然接梗，不确定时少用。")
                           : QStringLiteral("优先按用户给出的私人含义接梗，再参考公开语境。")},
                      {"avoid", QJsonArray()}, {"weight", shared ? 0.98 : 0.88}, {"enabled", true},
                      {"learned_at", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)}};
    m_learnedEntries.append(entry);
    m_entries.append(entry);
    persistLearnedMemes();
    emit learnedMemesChanged();
}

bool MemeCultureModule::handleLearningMessage(const QString &message, std::function<void(QString)> reply)
{
    if (!m_enabled) { m_lastUserMessage = message; return false; }
    const QString clean = message.trimmed();

    if (!m_pendingTerm.isEmpty()) {
        const bool noExplanation = clean.contains(QStringLiteral("不告诉")) || clean.contains(QStringLiteral("自己查"))
            || clean.contains(QStringLiteral("网上为准")) || clean.contains(QStringLiteral("不知道"))
            || clean.contains(QStringLiteral("你查到的为准"));
        const QString term = m_pendingTerm;
        saveLearnedMeme(term, m_pendingWebSummary, noExplanation ? QString() : clean, false);
        m_pendingTerm.clear(); m_pendingWebSummary.clear();
        QSettings settings; settings.remove("memes/pendingTerm"); settings.remove("memes/pendingWebSummary");
        m_lastUserMessage = clean;
        reply(noExplanation
              ? QStringLiteral("好，那我先按网上比较常见的意思记住“%1”。以后遇到新用法，我再慢慢校准。").arg(term)
              : QStringLiteral("记住啦。“%1”以后我会优先按你说的意思来接，同时拿网上的常见用法作参考——这算我们一起教会我的。").arg(term));
        return true;
    }

    const bool shared = clean.contains(QStringLiteral("以后这就叫")) || clean.contains(QStringLiteral("我们把这个叫"))
        || clean.contains(QStringLiteral("以后叫"));
    if (shared) {
        const QString term = inferMemeTerm(clean, m_lastUserMessage);
        if (!term.isEmpty()) {
            saveLearnedMeme(term, QStringLiteral("由用户和精灵在对话中共同约定的私人暗号。"),
                            m_lastUserMessage + QStringLiteral(" / ") + clean, true);
            m_lastUserMessage = clean;
            reply(QStringLiteral("成交，“%1”从现在起就是我们俩的共同梗。以后这个场景一出现，我会主动接暗号。").arg(term));
            return true;
        }
    }

    const bool saysMeme = clean.contains(QStringLiteral("这是一个梗")) || clean.contains(QStringLiteral("这是个梗"))
        || clean.contains(QStringLiteral("是一个梗")) || clean.contains(QStringLiteral("是个梗"));
    if (saysMeme) {
        const QString term = inferMemeTerm(clean, m_lastUserMessage);
        m_lastUserMessage = clean;
        if (term.isEmpty()) {
            reply(QStringLiteral("我知道你在给我补课了，但我没抓到梗词是哪几个字。把那个词用引号框一下告诉我，我就先去查。"));
            return true;
        }
        searchMeme(term, [this, term, reply = std::move(reply)](const QString &summary) mutable {
            m_pendingTerm = term;
            m_pendingWebSummary = summary;
            QSettings settings; settings.setValue("memes/pendingTerm", term); settings.setValue("memes/pendingWebSummary", summary);
            const QString found = summary.isEmpty()
                ? QStringLiteral("我刚刚查了一圈，但没找到足够可靠的公开解释")
                : QStringLiteral("我先查了一下，网上的说法大概是：%1").arg(summary.left(280));
            reply(QStringLiteral("%1。现在轮到你给我上课啦：你说“%2”时，具体是什么意思、通常在什么场景用？").arg(found, term));
        });
        return true;
    }

    m_lastUserMessage = clean;
    return false;
}

QStringList MemeCultureModule::learnedMemeSummaries() const
{
    QStringList result;
    for (const auto &value : m_learnedEntries) {
        const auto entry = value.toObject();
        const QString view = entry.value("user_view").toString();
        result << QStringLiteral("%1｜%2｜%3").arg(entry.value("phrase").toString(),
            entry.value("category").toString(), view.isEmpty() ? entry.value("meaning").toString().left(80) : view.left(80));
    }
    return result;
}

bool MemeCultureModule::removeLearnedMeme(int index)
{
    if (index < 0 || index >= m_learnedEntries.size()) return false;
    const QString id = m_learnedEntries[index].toObject().value("id").toString();
    m_learnedEntries.removeAt(index);
    for (int i = m_entries.size() - 1; i >= 0; --i)
        if (m_entries[i].toObject().value("id").toString() == id) m_entries.removeAt(i);
    persistLearnedMemes(); emit learnedMemesChanged(); return true;
}

void MemeCultureModule::enrichContext(const QString &message, const QString &base, std::function<void(QString)> done)
{
    QString combined = base;
    const QString local = contextForMessage(message);
    if (!local.isEmpty()) combined += "\n" + local;
    const QString key = searchKeyword(message);
    if (key.isEmpty()) { done(combined); return; }
    searchMeme(key, [combined, key, done = std::move(done)](const QString &summary) mutable {
        QString out = combined;
        if (!summary.isEmpty()) out += QStringLiteral(
            "\n[LIVE_MEME_SEARCH]\n仅搜索了梗词“%1”，近期搜索摘要：%2\n"
            "摘要可能有噪声，只用于理解语感。先接梗，再做与当前故事有关的变体，最后回到用户的具体内容；不要作百科解释。")
            .arg(key, summary);
        done(out);
    });
}

QString MemeCultureModule::contextForMessage(const QString &message)
{
    if (!m_enabled) return {};
    QList<QJsonObject> direct, related;
    const bool king = message.contains(QStringLiteral("王者")) || message.contains(QStringLiteral("巅峰赛"))
        || message.contains(QStringLiteral("排位"));
    for (const auto &value : m_entries) {
        const auto entry = value.toObject();
        if (!entry.value("enabled").toBool(true)) continue;
        bool exact = message.contains(entry.value("phrase").toString(), Qt::CaseInsensitive);
        for (const auto &alias : entry.value("aliases").toArray()) exact |= message.contains(alias.toString(), Qt::CaseInsensitive);
        if (exact) direct << entry;
        bool match = false;
        for (const auto &trigger : entry.value("triggers").toArray()) match |= message.contains(trigger.toString(), Qt::CaseInsensitive);
        const bool kingTerm = king && entry.value("category").toString() == QStringLiteral("游戏·王者荣耀")
            && entry.value("usage").toString() == "both";
        if (!exact && ((match && fresh(entry)) || kingTerm)) related << entry;
    }
    const bool proactive = recentCount() < 3 && QRandomGenerator::global()->bounded(100) < 30;
    if (direct.isEmpty() && !proactive) return {};
    QList<QJsonObject> candidates = direct;
    for (const auto &entry : related) candidates << entry;
    if (candidates.isEmpty()) return {};
    QStringList lines;
    for (int i = 0; i < candidates.size() && i < 3; ++i) {
        const auto entry = candidates[i];
        QString meaning = entry.value("meaning").toString();
        if (!entry.value("user_view").toString().isEmpty()) meaning += QStringLiteral("；用户的定义：") + entry.value("user_view").toString();
        lines << QStringLiteral("- %1：%2；%3").arg(entry.value("phrase").toString(), meaning, entry.value("style_hint").toString());
    }
    return QStringLiteral("[MEME_CULTURE]\n%1\n候选梗：\n%2\n最多自然使用一个；必须先回应用户本意。像熟人一样接梗，不解释出处、不罗列、不照抄；%3")
        .arg(direct.isEmpty() ? QStringLiteral("这是主动候选，不需要强行使用。") : QStringLiteral("用户已进入梗或圈内语境，请接住语感。"),
             lines.join('\n'), direct.isEmpty() ? QStringLiteral("不自然就不用。") : QStringLiteral("本轮优先自然带出相关表达，不要只复述原词。"));
}
