#include "DreamModule.h"
#include "../AiService.h"
#include "../logic/DreamGenerationRole.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QTime>

DreamModule::DreamModule(DataRepository *storage, AiService *ai, QObject *parent)
    : FeatureModule(parent), m_storage(storage), m_ai(ai)
{
    m_enabled = QSettings().value(QStringLiteral("modules/dreamEnabled"), true).toBool();
    refresh();
    connect(ai, &AiService::dreamCompleted, this, [this](const QJsonObject &raw) {
        if (!m_busy) return;
        m_busy = false;
        QJsonObject value;
        QString error;
        if (!DreamGenerationRole::normalizeAndValidate(raw, &value, &error)) {
            m_status = error;
            emit changed();
            return;
        }
        DreamRecord dream;
        dream.dreamDate = QDate::currentDate();
        dream.title = value.value(QStringLiteral("title")).toString();
        dream.content = value.value(QStringLiteral("content")).toString();
        dream.mood = value.value(QStringLiteral("mood")).toString();
        dream.dreamType = value.value(QStringLiteral("dream_type")).toString();
        for (const auto &item : value.value(QStringLiteral("symbols")).toArray()) dream.symbols << item.toString();
        dream.color = value.value(QStringLiteral("color")).toString();
        dream.realityHint = value.value(QStringLiteral("reality_hint")).toString();
        dream.continuationKey = value.value(QStringLiteral("continuation_key")).toString();
        for (const auto &item : value.value(QStringLiteral("memory_ids")).toArray()) dream.memoryIds << item.toVariant().toString();
        dream.createdAt = QDateTime::currentDateTime();
        if (m_storage->addDream(dream) > 0) {
            m_status = QStringLiteral("昨夜的梦已经折成一颗新的星星纸。它会安静地待在瓶子里。");
            refresh();
        } else {
            m_status = QStringLiteral("梦已经醒了，但星星纸没有成功放进瓶子。");
        }
        emit changed();
    });
    connect(ai, &AiService::dreamFailed, this, [this](const QString &error) {
        if (!m_busy) return;
        m_busy = false;
        m_status = error;
        emit changed();
    });
    connect(ai, &AiService::chatCompleted, this, [this](const QString &reply, const QString &, const QJsonObject &) {
        if (m_ai->requestContext() != QStringLiteral("dream_echo")) return;
        m_status = QStringLiteral("精灵已经看完你分享的内容。");
        emit echoResponseReady(reply);
        emit changed();
    });
    connect(ai, &AiService::chatFailed, this, [this](const QString &error) {
        if (m_ai->requestContext() != QStringLiteral("dream_echo")) return;
        m_status = QStringLiteral("分享已经保存，但精灵暂时没有组织好语言：%1").arg(error);
        emit changed();
    });
    m_timer.setInterval(30 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &DreamModule::evaluateDailyDream);
    m_timer.start();
    QTimer::singleShot(3500, this, &DreamModule::evaluateDailyDream);
}

void DreamModule::setEnabled(bool enabled)
{
    if (enabled == m_enabled) return;
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("modules/dreamEnabled"), enabled);
    emit enabledChanged(enabled);
    emit changed();
}

void DreamModule::refresh()
{
    const QList<DreamRecord> loaded = m_storage ? m_storage->loadDreams() : QList<DreamRecord>{};
    m_dreams.clear();
    m_dreams.reserve(loaded.size());
    for (const auto &dream : loaded) m_dreams.append(dream);
    if (m_selected >= m_dreams.size()) m_selected = m_dreams.isEmpty() ? -1 : 0;
    emit changed();
}

QStringList DreamModule::items() const
{
    QStringList result;
    for (const auto &dream : m_dreams)
        result << QStringLiteral("%1|%2|%3|%4|%5").arg(
            dream.dreamDate.toString(QStringLiteral("MM-dd")), dream.title, dream.color,
            dream.openedAt.isValid() ? QStringLiteral("opened") : QStringLiteral("sealed"),
            dream.favorite ? QStringLiteral("favorite") : QString());
    return result;
}

int DreamModule::unopenedCount() const
{
    int count = 0;
    for (const auto &dream : m_dreams) if (!dream.openedAt.isValid()) ++count;
    return count;
}

DreamRecord DreamModule::selectedDream() const
{
    return m_selected >= 0 && m_selected < m_dreams.size() ? m_dreams.at(m_selected) : DreamRecord{};
}

void DreamModule::evaluateDailyDream()
{
    if (!m_enabled || m_busy || !m_storage || QTime::currentTime() < QTime(6, 0)
        || m_storage->hasDreamForDate(QDate::currentDate())) return;
    startGeneration();
}

void DreamModule::requestTodayDream()
{
    if (!m_enabled) {
        m_status = QStringLiteral("梦境模块正在休息。开启后，明天的星星纸才会继续出现。");
        emit changed();
        return;
    }
    if (m_storage && m_storage->hasDreamForDate(QDate::currentDate())) {
        m_status = QStringLiteral("今天的星星纸已经在瓶子里了，不会重复生成。");
        emit changed();
        return;
    }
    if (QTime::currentTime() < QTime(6, 0)) {
        m_status = QStringLiteral("天还没亮，梦境要到早上六点后才会折成星星纸。");
        emit changed();
        return;
    }
    startGeneration();
}

void DreamModule::startGeneration()
{
    if (!m_ai->isConfigured()) {
        m_status = QStringLiteral("AI尚未配置，今晚的梦暂时没有折成纸。");
        emit changed();
        return;
    }
    if (m_ai->isBusy()) return;
    m_busy = true;
    m_status = QStringLiteral("精灵还在回想昨晚的梦……");
    emit changed();
    m_ai->generateDream(buildContext());
}

QJsonObject DreamModule::buildContext() const
{
    QJsonArray recent;
    const auto messages = m_storage->loadRecentMessages(40);
    for (int i = qMax(0, messages.size() - 12); i < messages.size(); ++i) {
        const auto &message = messages.at(i);
        recent.append(QJsonObject{{"sender", message.sender}, {"text", message.text.left(500)}, {"time", message.createdAt.toString(Qt::ISODate)}});
    }
    QJsonArray memories;
    int count = 0;
    for (const auto &memory : m_storage->loadMemories()) {
        if (memory.memoryState != QStringLiteral("active") || memory.deletedAt.isValid()) continue;
        memories.append(QJsonObject{{"id", QString::number(memory.id)}, {"subject", memory.subject}, {"content", memory.content.left(300)}});
        if (++count >= 8) break;
    }
    const auto state = m_storage->loadPetState();
    return QJsonObject{{"date", QDate::currentDate().toString(Qt::ISODate)},
                       {"season_month", QDate::currentDate().month()},
                       {"selected_theme", DreamGenerationRole::chooseTheme()},
                       {"pet_state", QJsonObject{{"mood", state.mood}, {"energy", state.energy}, {"closeness", state.closeness}, {"boredom", state.boredom}, {"curiosity", state.curiosity}}},
                       {"recent_fragments", recent}, {"candidate_memories", memories}};
}

void DreamModule::select(int row)
{
    if (row < 0 || row >= m_dreams.size()) return;
    m_selected = row;
    if (!m_dreams[row].openedAt.isValid()) m_storage->markDreamOpened(m_dreams[row].id, QDateTime::currentDateTime());
    refresh();
    m_selected = row;
    emit changed();
}

void DreamModule::toggleFavorite()
{
    if (m_selected < 0 || m_selected >= m_dreams.size()) return;
    m_storage->setDreamFavorite(m_dreams[m_selected].id, !m_dreams[m_selected].favorite);
    refresh();
}

void DreamModule::submitRealityEcho(const QString &text)
{
    if (m_selected < 0 || m_selected >= m_dreams.size() || text.trimmed().isEmpty()) return;
    const DreamRecord dream = m_dreams[m_selected];
    if (!m_storage->saveDreamRealityEcho(dream.id, text.trimmed())) {
        m_status = QStringLiteral("现实回声保存失败。"); emit changed(); return;
    }
    refresh();
    QList<ChatMessageRecord> history{{0, QStringLiteral("user"),
        QStringLiteral("用户主动分享了一段现实经历：%1\n把它当作普通聊天内容回应，先接住用户真正想分享的事情。禁止主动提及梦、星星纸、梦境相同或不同，也不能暗示你知道用户看过任何秘密内容。只围绕一个重点，自然回复50到100字，最多追问一个问题。").arg(text.trimmed().left(2000)),
        QDateTime::currentDateTime()}};
    const auto state = m_storage->loadPetState();
    m_ai->sendChat(history, state.mood, state.energy, state.health, state.closeness, state.boredom,
                   state.neglect, state.curiosity, state.irritation, QStringLiteral("dream_echo"), QString());
    m_status = QStringLiteral("现实回声已经悄悄送出……"); emit changed();
}

void DreamModule::submitVisualRealityEcho(const QJsonObject &vision, const QString &note)
{
    if (m_selected < 0 || m_selected >= m_dreams.size()) return;
    const DreamRecord dream = m_dreams[m_selected];
    QStringList matched;
    for (const auto &item : vision.value(QStringLiteral("matched_elements")).toArray()) matched << item.toString();
    const int match = qBound(0, vision.value(QStringLiteral("dream_match")).toInt(), 100);
    const QString echo = QStringLiteral("[照片回声] %1\n后台共鸣度：%2%\n对应元素：%3%4")
        .arg(vision.value(QStringLiteral("description")).toString()).arg(match)
        .arg(matched.join(QStringLiteral("、")), note.trimmed().isEmpty() ? QString() : QStringLiteral("\n用户补充：") + note.trimmed());
    if (!m_storage->saveDreamRealityEcho(dream.id, echo)) {
        m_status = QStringLiteral("照片看懂了，但现实回声保存失败。"); emit changed(); return;
    }
    refresh();
    const QString familiarity = match >= 80
        ? QStringLiteral("语气可以多一点说不清原因的惊喜和熟悉感，但不要解释来源；需要时只能说‘感觉有点熟悉’，不能提梦。")
        : match >= 50 ? QStringLiteral("语气可以温暖好奇，但不要强调神秘关联。")
                      : QStringLiteral("正常回应照片，不要强行制造关联。");
    QList<ChatMessageRecord> history{{0, QStringLiteral("user"),
        QStringLiteral("视觉模块确认照片内容：%1\n用户补充：%2\n请像真正看到用户主动分享的照片一样自然回应。先回应照片中最值得注意的一个事实，再表达感受，最多追问一个细节。%3\n绝对禁止提及梦、星星纸、梦境里有什么、与梦是否相同，也不能罗列识图报告或编造画面外的物体。事实与想象必须用‘像、让我想到、感觉’区分。回复50到100字，只生成一段。")
            .arg(vision.value(QStringLiteral("description")).toString(), note.left(500), familiarity), QDateTime::currentDateTime()}};
    const auto state = m_storage->loadPetState();
    m_ai->sendChat(history, state.mood, state.energy, state.health, state.closeness, state.boredom,
                   state.neglect, state.curiosity, state.irritation, QStringLiteral("dream_echo"), QString());
    m_status = QStringLiteral("精灵正在认真看这张照片……"); emit changed();
}

QString DreamModule::conversationContext(const QString &message, int closeness)
{
    if (!m_enabled || !m_storage) return {};
    const QRegularExpression question(QStringLiteral("(?:梦到|做梦|什么梦|梦见|昨晚.*梦|昨天.*梦|前天.*梦|最近.*梦)"));
    if (!question.match(message).hasMatch()) return {};
    QDate target;
    if (message.contains(QStringLiteral("前天"))) target = QDate::currentDate().addDays(-2);
    else if (message.contains(QStringLiteral("昨天")) || message.contains(QStringLiteral("昨晚"))) target = QDate::currentDate().addDays(-1);
    else if (message.contains(QStringLiteral("今天")) || message.contains(QStringLiteral("今早"))) target = QDate::currentDate();
    else {
        const QRegularExpression monthDay(QStringLiteral("(\\d{1,2})月(\\d{1,2})(?:日|号)?"));
        const auto match = monthDay.match(message);
        if (match.hasMatch()) target = QDate(QDate::currentDate().year(), match.captured(1).toInt(), match.captured(2).toInt());
    }
    DreamRecord dream;
    bool found = false;
    const auto all = m_storage->loadDreams();
    if (target.isValid()) {
        for (const auto &candidate : all) if (candidate.dreamDate == target) { dream = candidate; found = true; break; }
    } else if (!all.isEmpty()) {
        dream = all.first(); target = dream.dreamDate; found = true;
    }
    if (!found)
        return QStringLiteral("[DREAM_CANON] 用户正在询问%1的梦，但数据库没有该日期的梦境记录。必须诚实说那天没有留下梦或醒来后想不起来，禁止临时编造梦境。")
            .arg(target.isValid() ? target.toString(QStringLiteral("yyyy年M月d日")) : QStringLiteral("最近"));
    int level = dream.disclosureLevel;
    if (level == 0) {
        const int fullChance = qBound(15, 15 + closeness / 10, 25);
        const int roll = QRandomGenerator::global()->bounded(100);
        level = roll < fullChance ? 3 : (roll < 70 ? 2 : 1);
        m_storage->setDreamDisclosure(dream.id, level, QDateTime::currentDateTime());
        refresh();
    }
    const QString base = QStringLiteral("[DREAM_CANON] 用户询问的是%1的梦。数据库中的唯一真实梦境标题为《%2》。这条记录是唯一事实来源；不得生成另一场梦，不得声称知道用户是否打开过星星纸。打开星星纸是用户侧不可见操作。 ")
        .arg(dream.dreamDate.toString(QStringLiteral("yyyy年M月d日")), dream.title);
    if (level == 1)
        return base + QStringLiteral("本次精灵不愿透露内容。可以承认做过梦并卖个小关子，但不得泄露具体情节。以后复述时也不能编造另一版本。");
    if (level == 2)
        return base + QStringLiteral("本次只允许透露这些真实片段：%1。可用不确定、刚睡醒的语气表达，但禁止补充片段之外的新情节，不要完整讲述。").arg(dream.symbols.mid(0, 2).join(QStringLiteral("、")));
    return base + QStringLiteral("这场梦已经允许完整讲述。真实正文：%1。可以换自然措辞，但人物、物品、因果和结局必须保持一致，不得加入第二个版本。").arg(dream.content);
}

QString DreamModule::visionContext() const
{
    QStringList lines;
    int count = 0;
    for (const auto &dream : m_dreams) {
        lines << QStringLiteral("[%1]《%2》：%3；元素：%4")
                     .arg(dream.dreamDate.toString(Qt::ISODate), dream.title,
                          dream.content.left(500), dream.symbols.join(QStringLiteral("、")));
        if (++count >= 7) break;
    }
    return lines.join(QLatin1Char('\n'));
}
