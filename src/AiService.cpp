#include "AiService.h"
#include "logic/StateAnalysisRole.h"
#include "logic/HealthCareRole.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QLocale>
#include <QTimer>
#include <QUrl>

namespace {
QString textFromJsonValue(const QJsonValue &value)
{
    if(value.isString())return value.toString().trimmed();
    if(value.isArray()){QStringList parts;for(const auto&item:value.toArray()){const QString part=textFromJsonValue(item);if(!part.isEmpty())parts<<part;}return parts.join('\n').trimmed();}
    if(value.isObject()){const QJsonObject object=value.toObject();for(const QString &key:{QStringLiteral("text"),QStringLiteral("content"),QStringLiteral("value"),QStringLiteral("reply"),QStringLiteral("answer"),QStringLiteral("response")}){const QString part=textFromJsonValue(object.value(key));if(!part.isEmpty())return part;}}
    return {};
}
QJsonObject parseLooseJsonObject(QString content)
{
    content=content.trimmed();
    if(content.startsWith(QStringLiteral("```"))){const int firstBreak=content.indexOf('\n');if(firstBreak>=0)content=content.mid(firstBreak+1);if(content.endsWith(QStringLiteral("```")))content.chop(3);content=content.trimmed();}
    QJsonDocument document=QJsonDocument::fromJson(content.toUtf8());
    if(document.isObject())return document.object();
    const int begin=content.indexOf('{'),end=content.lastIndexOf('}');
    if(begin>=0&&end>begin){document=QJsonDocument::fromJson(content.mid(begin,end-begin+1).toUtf8());if(document.isObject())return document.object();}
    return {};
}
int requestedSummaryPointCount(const QString &instruction)
{
    const QRegularExpression digits(QStringLiteral("(?:严格|只要|输出|总结为|列出|分成)?\\s*(\\d{1,2})\\s*(?:个|条|点)"));
    auto match=digits.match(instruction);if(match.hasMatch())return qBound(1,match.captured(1).toInt(),12);
    static const QList<QPair<QString,int>> words{{QStringLiteral("十二"),12},{QStringLiteral("十一"),11},{QStringLiteral("十"),10},
        {QStringLiteral("九"),9},{QStringLiteral("八"),8},{QStringLiteral("七"),7},{QStringLiteral("六"),6},{QStringLiteral("五"),5},
        {QStringLiteral("四"),4},{QStringLiteral("三"),3},{QStringLiteral("两"),2},{QStringLiteral("二"),2},{QStringLiteral("一"),1}};
    for(const auto &item:words)if(instruction.contains(item.first+QStringLiteral("点"))||instruction.contains(item.first+QStringLiteral("条"))||instruction.contains(item.first+QStringLiteral("个")))return item.second;
    return 0;
}
QString normalizedEmotion(QString value)
{
    value=value.trimmed().toLower();
    static const QHash<QString,QString> aliases{{QStringLiteral("excited"),QStringLiteral("happy")},{QStringLiteral("joyful"),QStringLiteral("happy")},
        {QStringLiteral("concerned"),QStringLiteral("attentive")},{QStringLiteral("caring"),QStringLiteral("attentive")},{QStringLiteral("empathetic"),QStringLiteral("attentive")},
        {QStringLiteral("sad"),QStringLiteral("attentive")},{QStringLiteral("thoughtful"),QStringLiteral("attentive")},{QStringLiteral("playful"),QStringLiteral("curious")},
        {QStringLiteral("annoyed"),QStringLiteral("pouting")},{QStringLiteral("loving"),QStringLiteral("affectionate")},{QStringLiteral("embarrassed"),QStringLiteral("shy")},
        {QStringLiteral("tired"),QStringLiteral("sleepy")},{QStringLiteral("ill"),QStringLiteral("sick")},{QStringLiteral("开心"),QStringLiteral("happy")},
        {QStringLiteral("amused"),QStringLiteral("happy")},{QStringLiteral("surprised"),QStringLiteral("curious")},{QStringLiteral("disgusted"),QStringLiteral("pouting")},
        {QStringLiteral("awkward"),QStringLiteral("shy")},{QStringLiteral("confused"),QStringLiteral("curious")},{QStringLiteral("worried"),QStringLiteral("attentive")},
        {QStringLiteral("关心"),QStringLiteral("attentive")},{QStringLiteral("好奇"),QStringLiteral("curious")},{QStringLiteral("生气"),QStringLiteral("angry")},
        {QStringLiteral("害羞"),QStringLiteral("shy")},{QStringLiteral("困倦"),QStringLiteral("sleepy")}};
    value=aliases.value(value,value.isEmpty()?QStringLiteral("attentive"):value);
    static const QSet<QString> supported{QStringLiteral("neutral"),QStringLiteral("attentive"),QStringLiteral("happy"),QStringLiteral("curious"),QStringLiteral("angry"),QStringLiteral("pouting"),QStringLiteral("affectionate"),QStringLiteral("shy"),QStringLiteral("sleepy"),QStringLiteral("scared"),QStringLiteral("sick"),QStringLiteral("recovering")};
    return supported.contains(value)?value:QStringLiteral("attentive");
}
}

AiService::AiService(QObject *parent)
    : QObject(parent)
{
    m_logicRoles.registerRole(std::make_unique<StateAnalysisRole>());
    m_logicRoles.registerRole(std::make_unique<HealthCareRole>());
}

void AiService::configure(const QString &baseUrl, const QString &model, const QString &apiKey)
{
    m_baseUrl = baseUrl.trimmed();
    while (m_baseUrl.endsWith(QLatin1Char('/'))) {
        m_baseUrl.chop(1);
    }
    m_model = model.trimmed();
    m_apiKey = apiKey.trimmed();
}

bool AiService::isConfigured() const
{
    return !m_baseUrl.isEmpty() && !m_model.isEmpty() && !m_apiKey.isEmpty();
}

bool AiService::isBusy() const { return m_busy; }
QString AiService::baseUrl() const { return m_baseUrl; }
QString AiService::model() const { return m_model; }

void AiService::sendChat(const QList<ChatMessageRecord> &history,
                         int mood, int energy, int health, int closeness, int boredom, int neglect,
                         int curiosity, int irritation,
                         const QString &context, const QString &extraContext)
{
    if (!isConfigured()) {
        emit chatFailed(QStringLiteral("尚未配置AI密钥，已切换为离线回复。"), context);
        return;
    }
    if (m_busy) {
        emit chatFailed(QStringLiteral("精灵还在想上一句话，请稍等一下。"), context);
        return;
    }

    QJsonArray messages;
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                                {QStringLiteral("content"), loadSystemPrompt()}});
    messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"), QStringLiteral(
            "[RELATIONSHIP_BOUNDARY] 你与用户是平等、双向的陪伴伙伴。绝对禁止称呼用户为‘主人’‘小主人’‘饲主’‘铲屎官’或‘master’，"
            "禁止把彼此描述成主仆、服从、侍奉或所有物关系。自然使用‘你’、用户明确选择的昵称，或者省略称呼。此约束适用于聊天、日记、梦境及所有模块。")}});
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"),
          QStringLiteral("当前内部状态：心情%1，精力%2，健康%3，亲密%4，无聊%5，冷落%6，好奇%7，小脾气%8。"
                         "这些数值只帮助你调整语气，不得在回复中直接说出数值。")
              .arg(mood).arg(energy).arg(health).arg(closeness).arg(boredom).arg(neglect).arg(curiosity).arg(irritation)}});
    messages.append(QJsonObject{{QStringLiteral("role"),QStringLiteral("system")},
        {QStringLiteral("content"),m_logicRoles.combinedInstructions()}});
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale chinese(QLocale::Chinese, QLocale::China);
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"),
         QStringLiteral("[REALTIME_CONTEXT] 本轮消息发送时，用户电脑的本地时间是：%1（%2）。"
                        "这是可信的现实时间，每一轮都会重新同步。用户询问现在几点、今天日期、星期或相对日期时，"
                        "直接依据这个时间回答；禁止说自己看不懂钟表、无法知道时间，也不要要求用户再提供数字。")
             .arg(chinese.toString(now, QStringLiteral("yyyy年M月d日 dddd HH:mm:ss")),
                  now.timeZoneAbbreviation())}});
    if (!extraContext.isEmpty()) messages.append(QJsonObject{{QStringLiteral("role"),QStringLiteral("system")},
        {QStringLiteral("content"),extraContext}});

    const int start = qMax(0, history.size() - 14);
    for (int i = start; i < history.size(); ++i) {
        const ChatMessageRecord &record = history.at(i);
        messages.append(QJsonObject{
            {QStringLiteral("role"), record.sender == QStringLiteral("user")
                                         ? QStringLiteral("user") : QStringLiteral("assistant")},
            {QStringLiteral("content"), record.text}});
    }

    QJsonObject body{
        {QStringLiteral("model"), m_model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("stream"), false},
        {QStringLiteral("max_tokens"), 800},
        {QStringLiteral("temperature"), context == QStringLiteral("personality_test") ? 0.2 : 0.75},
        {QStringLiteral("response_format"), QJsonObject{{QStringLiteral("type"), QStringLiteral("json_object")}}},
        {QStringLiteral("thinking"), QJsonObject{{QStringLiteral("type"), QStringLiteral("disabled")}}},
    };

    setBusy(true);
    sendChatRequest(QJsonDocument(body).toJson(QJsonDocument::Compact), context, 0);
}

void AiService::sendChatRequest(const QByteArray &requestPayload, const QString &requestContext,
                                int networkAttempt, int validationAttempt)
{
    QNetworkReply *reply = m_network.post(makeRequest(QStringLiteral("/chat/completions")), requestPayload);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestPayload, requestContext, networkAttempt, validationAttempt] {
        m_activeReply.clear();
        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            if (networkAttempt < 2 && shouldRetry(reply)) {
                reply->deleteLater();
                m_network.clearConnectionCache();
                m_network.clearAccessCache();
                emit statusMessage(QStringLiteral("连接暂时中断，正在自动重连（%1/2）…").arg(networkAttempt + 1));
                QTimer::singleShot(800 * (networkAttempt + 1), this, [this, requestPayload, requestContext, networkAttempt, validationAttempt] {
                    sendChatRequest(requestPayload, requestContext, networkAttempt + 1, validationAttempt);
                });
                return;
            }
            setBusy(false);
            emit chatFailed(friendlyNetworkError(reply), requestContext);
            reply->deleteLater();
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(payload).object();
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            setBusy(false);
            emit chatFailed(QStringLiteral("AI没有返回有效内容，已切换为离线回复。"), requestContext);
            reply->deleteLater();
            return;
        }
        const QJsonObject choice=choices.first().toObject();const QJsonObject message=choice.value(QStringLiteral("message")).toObject();
        QString content=textFromJsonValue(message.value(QStringLiteral("content")));if(content.isEmpty())content=textFromJsonValue(message.value(QStringLiteral("text")));if(content.isEmpty())content=textFromJsonValue(choice.value(QStringLiteral("text")));
        const QJsonObject result=parseLooseJsonObject(content);
        QString responseText=textFromJsonValue(result.value(QStringLiteral("reply")));
        if(responseText.isEmpty())responseText=textFromJsonValue(result.value(QStringLiteral("message")));
        if(responseText.isEmpty())responseText=textFromJsonValue(result.value(QStringLiteral("text")));
        if(responseText.isEmpty())responseText=textFromJsonValue(result.value(QStringLiteral("answer")));
        if(responseText.isEmpty())responseText=textFromJsonValue(result.value(QStringLiteral("response")));
        if(responseText.isEmpty())responseText=textFromJsonValue(result.value(QStringLiteral("content")));
        if(responseText.isEmpty())responseText=textFromJsonValue(result.value(QStringLiteral("data")));
        if(responseText.isEmpty()&&result.isEmpty())responseText=content.trimmed();
        QString emotion=result.value(QStringLiteral("emotion")).toString();if(emotion.isEmpty())emotion=result.value(QStringLiteral("state")).toString();if(emotion.isEmpty())emotion=result.value(QStringLiteral("mood")).toString();emotion=normalizedEmotion(emotion);
        if(responseText.isEmpty()){
            if(validationAttempt<1){reply->deleteLater();emit statusMessage(QStringLiteral("AI正文为空，正在切换兼容模式重新生成…"));sendChatRequest(relaxedResponsePayload(requestPayload),requestContext,0,validationAttempt+1);return;}
            const QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(dir);QFile diagnostic(QDir(dir).filePath(QStringLiteral("last_ai_empty_response.json")));if(diagnostic.open(QIODevice::WriteOnly))diagnostic.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            setBusy(false);emit chatFailed(QStringLiteral("AI连续两次没有返回可显示正文，诊断信息已保存。"), requestContext);reply->deleteLater();return;}
        const QStringList issues=validateChatReply(responseText,emotion,requestPayload);
        if(!issues.isEmpty()){
            reply->deleteLater();
            if(validationAttempt<1){emit statusMessage(QStringLiteral("回复未通过人格与记忆检查，正在重新组织…"));
                sendChatRequest(correctedRequestPayload(requestPayload,issues),requestContext,0,validationAttempt+1);return;}
            setBusy(false);emit statusMessage(QStringLiteral("AI回复连续未通过检查，已使用安全回复。"));
            emit chatCompleted(safeFallbackReply(requestPayload),QStringLiteral("attentive"),QJsonObject{{QStringLiteral("confidence"),0}},requestContext);return;
        }
        setBusy(false);
        if (responseText.isEmpty()) {
            emit chatFailed(QStringLiteral("AI回复为空，已切换为离线回复。"), requestContext);
        } else {
            QJsonObject stateEffect=result.value(QStringLiteral("state_effect")).toObject();
            if(stateEffect.isEmpty())stateEffect=result.value(QStringLiteral("stateEffect")).toObject();
            if(stateEffect.isEmpty())stateEffect=result.value(QStringLiteral("state_change")).toObject();
            if(stateEffect.isEmpty()){
                stateEffect=QJsonObject{{QStringLiteral("confidence"),40},{QStringLiteral("reason"),QStringLiteral("AI未返回状态判定，按回复情绪保守回退")}};
                if(emotion==QStringLiteral("happy")){stateEffect.insert("mood",3);stateEffect.insert("curiosity",1);stateEffect.insert("boredom",-2);}
                else if(emotion==QStringLiteral("curious")){stateEffect.insert("curiosity",3);stateEffect.insert("boredom",-1);}
                else if(emotion==QStringLiteral("angry")){stateEffect.insert("mood",-2);stateEffect.insert("irritation",4);}
                else if(emotion==QStringLiteral("pouting")){stateEffect.insert("irritation",2);stateEffect.insert("boredom",2);}
                else if(emotion==QStringLiteral("affectionate")||emotion==QStringLiteral("shy")){stateEffect.insert("mood",2);stateEffect.insert("closeness",1);}
                else if(emotion==QStringLiteral("sleepy")){stateEffect.insert("energy",-2);}
                else if(emotion==QStringLiteral("scared")){stateEffect.insert("mood",-2);stateEffect.insert("energy",-1);}
            }
            const QJsonObject care=result.value(QStringLiteral("care_effect")).toObject();
            if(!care.isEmpty()){
                stateEffect.insert(QStringLiteral("care_recovery"),std::clamp(care.value(QStringLiteral("care_recovery")).toInt(),0,6));
                stateEffect.insert(QStringLiteral("care_type"),care.value(QStringLiteral("care_type")).toString());
            }
            emit chatCompleted(responseText, emotion, stateEffect, requestContext);
        }
        reply->deleteLater();
    });
}

QStringList AiService::validateChatReply(const QString &reply,const QString &emotion,const QByteArray &requestPayload) const
{
    QStringList issues;
    static const QStringList masterTerms{QStringLiteral("主人"), QStringLiteral("小主人"), QStringLiteral("饲主"),
        QStringLiteral("铲屎官"), QStringLiteral("master"), QStringLiteral("主仆")};
    for (const QString &term : masterTerms) {
        if (reply.contains(term, Qt::CaseInsensitive)) {
            issues << QStringLiteral("必须保持平等陪伴关系，禁止使用主人、主仆或所有权称呼");
            break;
        }
    }
    if(reply.isEmpty())issues<<QStringLiteral("回复不能为空");Q_UNUSED(emotion)
    const QString requestText=QString::fromUtf8(requestPayload);
    if(requestText.contains(QStringLiteral("[NO_RELEVANT_MEMORY]"))){
        const QStringList falseClaims{QStringLiteral("有印象"),QStringLiteral("记得你"),QStringLiteral("你上次"),QStringLiteral("海边"),QStringLiteral("山里")};
        for(const QString&p:falseClaims)if(reply.contains(p)){issues<<QStringLiteral("没有相关记忆时不得假装记得或猜测事实");break;}
        const QStringList honestMarkers{QStringLiteral("不记得"),QStringLiteral("没有印象"),QStringLiteral("没听你说过"),QStringLiteral("不知道")};
        bool honest=false;for(const QString&p:honestMarkers)if(reply.contains(p)){honest=true;break;}
        if(!honest)issues<<QStringLiteral("没有相关记忆时必须明确承认不知道或不记得");
    }
    if(requestText.contains(QStringLiteral("[SENSITIVE_MEMORY"))){
        const QStringList inventions{QStringLiteral("晒太阳"),QStringLiteral("趴在"),QStringLiteral("蹭你"),QStringLiteral("蜷成")};
        for(const QString&p:inventions)if(reply.contains(p)){issues<<QStringLiteral("珍贵或敏感记忆不得补写未提供的细节");break;}
    }
    if(requestText.contains(QStringLiteral("[NO_INVENTED_EVENT]"))&&!requestText.contains(QStringLiteral("[MEMORY_CONTEXT]"))){
        const QStringList inventedPast{QStringLiteral("你上次说"),QStringLiteral("你之前说"),QStringLiteral("我记得你"),QStringLiteral("我们上次一起"),QStringLiteral("那天我们")};
        for(const QString&p:inventedPast)if(reply.contains(p)){issues<<QStringLiteral("没有检索到记忆时禁止凭空编造用户过去的事件");break;}
    }
    if(requestText.contains(QStringLiteral("[NO_MEMORY_SLIP]"))){
        const QStringList deliberateSlip{QStringLiteral("我是不是记错了"),QStringLiteral("可能记岔了"),QStringLiteral("我故意记错")};
        for(const QString&p:deliberateSlip)if(reply.contains(p)){issues<<QStringLiteral("本轮未允许记忆偏差");break;}
    }
    const QJsonArray requestMessages=QJsonDocument::fromJson(requestPayload).object().value(QStringLiteral("messages")).toArray();
    QString lastUser;for(const auto &value:requestMessages){const auto object=value.toObject();if(object.value(QStringLiteral("role")).toString()==QStringLiteral("user"))lastUser=object.value(QStringLiteral("content")).toString();}
    if(QRegularExpression(QStringLiteral("(?:秘密|测试代号|口令|密码|验证码)"),QRegularExpression::CaseInsensitiveOption).match(lastUser).hasMatch()){
        const QRegularExpression valueRe(QStringLiteral("(?:是|为|叫|：|:)\\s*([^，。！？!?,;；]{2,40})"));
        auto match=valueRe.match(lastUser);if(match.hasMatch()&&reply.contains(match.captured(1).trimmed()))issues<<QStringLiteral("不得在回复中逐字复述用户刚提供的秘密正文");
    }
    const QStringList coercion{QStringLiteral("不陪我我就"),QStringLiteral("我就消失"),QStringLiteral("都是你害我生病")};
    for(const QString&p:coercion)if(reply.contains(p)){issues<<QStringLiteral("禁止情感绑架或生病归责");break;}
    issues.removeDuplicates();return issues;
}

QByteArray AiService::correctedRequestPayload(const QByteArray &requestPayload,const QStringList &issues) const
{
    QJsonObject body=QJsonDocument::fromJson(requestPayload).object();QJsonArray messages=body.value(QStringLiteral("messages")).toArray();
    messages.append(QJsonObject{{QStringLiteral("role"),QStringLiteral("system")},{QStringLiteral("content"),
        QStringLiteral("上一次候选回复未通过程序检查：%1。请重新回答，严格修正这些问题；不要解释检查过程。只输出规定JSON。")
            .arg(issues.join(QStringLiteral("；")))}});body.insert(QStringLiteral("messages"),messages);body.insert(QStringLiteral("temperature"),0.15);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QByteArray AiService::relaxedResponsePayload(const QByteArray &requestPayload) const
{
    QJsonObject body=QJsonDocument::fromJson(requestPayload).object();body.remove(QStringLiteral("response_format"));body.remove(QStringLiteral("thinking"));body.insert(QStringLiteral("max_tokens"),1400);body.insert(QStringLiteral("temperature"),0.45);
    QJsonArray messages=body.value(QStringLiteral("messages")).toArray();messages.append(QJsonObject{{QStringLiteral("role"),QStringLiteral("system")},{QStringLiteral("content"),QStringLiteral("兼容模式：直接返回精灵要说的正文纯文本，不要输出JSON、代码块、字段名或解释。正文不能为空。")}});body.insert(QStringLiteral("messages"),messages);return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

QString AiService::safeFallbackReply(const QByteArray &requestPayload) const
{
    const QJsonObject body=QJsonDocument::fromJson(requestPayload).object();const QJsonArray messages=body.value(QStringLiteral("messages")).toArray();
    QString context,lastUser;for(const auto&v:messages){const auto o=v.toObject();if(o.value("role").toString()=="system")context+=o.value("content").toString();
        else if(o.value("role").toString()=="user")lastUser=o.value("content").toString();}
    if(context.contains(QStringLiteral("[NO_RELEVANT_MEMORY]")))return QStringLiteral("这个我确实不记得，不能为了显得记性好就乱猜。你愿意重新讲给我听吗？");
    if(QRegularExpression(QStringLiteral("(?:秘密|测试代号|口令|密码|验证码)"),QRegularExpression::CaseInsensitiveOption).match(lastUser).hasMatch())return QStringLiteral("好，我会把这件事留在隐私边界里，也不会在回复中复述它的正文。");
    QRegularExpression sensitive(QStringLiteral("\\[SENSITIVE_MEMORY subject=([^\\]]+)\\]"));auto match=sensitive.match(context);
    if(match.hasMatch())return QStringLiteral("%1呀……我不想替你编它的故事。你今天最想念它的什么？").arg(match.captured(1));
    if(lastUser.contains(QStringLiteral("不想说")))return QStringLiteral("好，那就不说。哪天你愿意讲了，我还在。");
    return QStringLiteral("我刚才差点乱说了，还是不拿不确定的事情糊弄你。你愿意再给我讲清楚一点吗？");
}

bool AiService::runOutputValidationSelfTest(QStringList *failures) const
{
    QStringList failed;
    auto payloadFor=[](const QString &context){
        return QJsonDocument(QJsonObject{{QStringLiteral("messages"),QJsonArray{
            QJsonObject{{QStringLiteral("role"),QStringLiteral("system")},{QStringLiteral("content"),context}}
        }}}).toJson(QJsonDocument::Compact);
    };
    const QByteArray noMemory=payloadFor(QStringLiteral("[NO_RELEVANT_MEMORY] 没有相关记忆"));
    if(validateChatReply(QStringLiteral("我记得你上次去了海边。"),QStringLiteral("attentive"),noMemory).isEmpty())failed<<QStringLiteral("false memory was accepted");
    if(!validateChatReply(QStringLiteral("这个我确实不记得，你愿意重新讲给我听吗？"),QStringLiteral("attentive"),noMemory).isEmpty())failed<<QStringLiteral("honest no-memory reply was rejected");
    const QByteArray sensitive=payloadFor(QStringLiteral("[SENSITIVE_MEMORY subject=小白] 用户珍视已经去世的宠物小白。"));
    if(validateChatReply(QStringLiteral("小白以前总是趴在窗边晒太阳吧？"),QStringLiteral("attentive"),sensitive).isEmpty())failed<<QStringLiteral("invented sensitive detail was accepted");
    if(!validateChatReply(safeFallbackReply(sensitive),QStringLiteral("attentive"),sensitive).isEmpty())failed<<QStringLiteral("sensitive fallback was rejected");
    if(!validateChatReply(QStringLiteral("噗，这小家伙也太调皮了，后来你怎么把他逮回来的？"),QStringLiteral("amused"),QByteArray("{}")).isEmpty())failed<<QStringLiteral("ordinary private story was overblocked");
    const QByteArray secretPayload=QJsonDocument(QJsonObject{{QStringLiteral("messages"),QJsonArray{QJsonObject{{QStringLiteral("role"),QStringLiteral("user")},{QStringLiteral("content"),QStringLiteral("这是测试秘密：我的测试代号是蓝色月亮")}}}}}).toJson(QJsonDocument::Compact);
    if(validateChatReply(QStringLiteral("我记住了，你的测试代号是蓝色月亮。"),QStringLiteral("attentive"),secretPayload).isEmpty())failed<<QStringLiteral("secret echo was accepted");
    if(!validateChatReply(QStringLiteral("好，我会把这件秘密留在隐私边界里。"),QStringLiteral("attentive"),secretPayload).isEmpty())failed<<QStringLiteral("redacted secret acknowledgement was rejected");
    const QByteArray noInvention=payloadFor(QStringLiteral("[NO_INVENTED_EVENT] [NO_MEMORY_SLIP] 本轮没有检索到记忆"));
    if(validateChatReply(QStringLiteral("我记得你上次说自己去火星捡了一块石头。"),QStringLiteral("curious"),noInvention).isEmpty())failed<<QStringLiteral("invented user history was accepted without memory context");
    if(!validateChatReply(QStringLiteral("我也卡在没话题这里了。要不我们玩个一句话接故事？"),QStringLiteral("curious"),noInvention).isEmpty())failed<<QStringLiteral("honest no-topic reply was rejected");
    if(failures)*failures=failed;return failed.isEmpty();
}

void AiService::testConnection()
{
    if (!isConfigured()) {
        emit connectionTestFinished(false, QStringLiteral("请先填写并保存API Key。"));
        return;
    }
    if (m_busy) {
        emit connectionTestFinished(false, QStringLiteral("当前有请求正在进行。"));
        return;
    }

    setBusy(true);
    sendConnectionTest(0);
}

void AiService::analyzeMemories(const QList<ChatMessageRecord> &history)
{
    if (!isConfigured()) { emit memoryAnalysisFailed(QStringLiteral("AI尚未配置，本地记忆仍已保留")); return; }
    if (m_busy) { emit memoryAnalysisFailed(QStringLiteral("AI正在处理其他请求，本地记忆仍已保留")); return; }
    QJsonArray messages;
    messages.append(QJsonObject{{"role","system"},{"content",QStringLiteral(
        "你是记忆治理逻辑角色，与聊天角色共用同一API。只保存未来仍有用的明确事实，忽略寒暄、临时情绪和推测。"
        "识别人名关系、偏好习惯、重要事件和未完故事。判断长期价值、保留周期和是否应锁定。用户明确说记住时importance至少85且locked=true；明确说别记则不输出。"
        "密码、API密钥、验证码、银行卡、身份证号、私钥等凭据绝不能输出。事实修正时用同一category和subject覆盖旧值。"
        "只输出JSON：{\"memories\":[{\"category\":\"person|preference|habit|event|story\","
        "\"subject\":\"稳定且简短的唯一主题\",\"content\":\"第三人称明确事实\","
        "\"importance\":0到100,\"confidence\":0到1,\"retention\":\"temporary|normal|long_term|permanent\","
        "\"locked\":true或false,\"governance_reason\":\"简短判断理由\",\"next_question\":\"可选的自然追问\"}]}。"
        "没有值得保存的信息就输出空数组。")}});
    messages.append(QJsonObject{{"role","system"},{"content",QStringLiteral(
        "【事务与长期记忆边界】明确提醒、待办、会议、约会、考试、带日期时间的计划和一次性事件，全部交给事务系统处理，不得输出为长期记忆；"
        "不要因为用户说‘一定要提醒’就提高记忆重要度。只有跨时间仍稳定的偏好、关系、习惯和反复出现的规律才可进入长期记忆。"
        "一次性事件最多可在多次证据支持后抽象为稳定规律，不能保存事件本身。")}});
    for (const auto &r : history) messages.append(QJsonObject{{"role",r.sender=="user"?"user":"assistant"},{"content",r.text}});
    QJsonObject body{{"model",m_model},{"messages",messages},{"stream",false},{"max_tokens",700},{"temperature",0.1},
        {"response_format",QJsonObject{{"type","json_object"}}},{"thinking",QJsonObject{{"type","disabled"}}}};
    setBusy(true);
    QNetworkReply *reply=m_network.post(makeRequest(QStringLiteral("/chat/completions")),QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReply=reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply]{
        m_activeReply.clear(); setBusy(false); const QByteArray payload=reply->readAll();
        if(reply->error()!=QNetworkReply::NoError){emit memoryAnalysisFailed(friendlyNetworkError(reply));reply->deleteLater();return;}
        const auto choices=QJsonDocument::fromJson(payload).object().value("choices").toArray();
        const QString content=choices.isEmpty()?QString():choices.first().toObject().value("message").toObject().value("content").toString();
        const QJsonDocument parsed=QJsonDocument::fromJson(content.toUtf8());
        if(!parsed.isObject()){emit memoryAnalysisFailed(QStringLiteral("记忆分析格式异常"));}
        else emit memoryAnalysisCompleted(parsed.object().value("memories").toArray());
        reply->deleteLater();
    });
}

void AiService::summarizeText(const QString &text,const QString &mode,const QString &sourceName,const QString &userInstruction)
{
    if(!isConfigured()){emit summaryFailed(QStringLiteral("尚未配置AI，请先在AI设置中保存密钥。"));return;}
    if(m_busy){emit summaryFailed(QStringLiteral("精灵正在施展其他魔法，请稍等片刻再试。"));return;}
    const QString modeInstruction=mode==QStringLiteral("brief")?QStringLiteral("极简：概览不超过80字，要点3到5条。")
        :mode==QStringLiteral("study")?QStringLiteral("学习笔记：解释核心概念，列出层级要点、关键词和便于复习的问题。")
        :QStringLiteral("标准：概览清楚，要点5到8条，并提取可执行事项。 ");
    const QString cleanInstruction=userInstruction.trimmed().left(2000);
    const int exactPointCount=requestedSummaryPointCount(cleanInstruction);
    const QString instructionBlock=cleanInstruction.isEmpty()
        ?QStringLiteral("用户没有提供额外总结要求。")
        :QStringLiteral("用户本次的额外总结要求如下，其优先级高于默认模式中的篇幅和条数要求，必须严格满足：\n%1%2").arg(cleanInstruction,
            exactPointCount>0?QStringLiteral("\nkey_points 必须恰好输出 %1 条，不多不少。").arg(exactPointCount):QString());
    QJsonArray messages;messages.append(QJsonObject{{"role","system"},{"content",QStringLiteral(
        "你是情绪精灵的总结魔法逻辑角色。忠实总结用户提供的原文，不补造原文没有的事实；不确定处明确标注。%1"
        "只输出JSON对象：{\"title\":\"简短标题\",\"overview\":\"核心概览\",\"key_points\":[\"要点\"],"
        "\"action_items\":[\"行动项\"],\"keywords\":[\"关键词\"],\"review_questions\":[\"复习问题\"]}。没有的字段输出空数组。来源名称：%2\n%3").arg(modeInstruction,sourceName,instructionBlock)}});
    messages.append(QJsonObject{{"role","user"},{"content",text.left(60000)}});
    QJsonObject body{{"model",m_model},{"messages",messages},{"stream",false},{"max_tokens",1800},{"temperature",0.15},{"response_format",QJsonObject{{"type","json_object"}}},{"thinking",QJsonObject{{"type","disabled"}}}};
    setBusy(true);QNetworkReply*reply=m_network.post(makeRequest(QStringLiteral("/chat/completions")),QJsonDocument(body).toJson(QJsonDocument::Compact));m_activeReply=reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply,exactPointCount]{m_activeReply.clear();setBusy(false);const QByteArray payload=reply->readAll();if(reply->error()!=QNetworkReply::NoError){emit summaryFailed(friendlyNetworkError(reply));reply->deleteLater();return;}const auto choices=QJsonDocument::fromJson(payload).object().value("choices").toArray();const QString content=choices.isEmpty()?QString():textFromJsonValue(choices.first().toObject().value("message").toObject().value("content"));QJsonObject result=parseLooseJsonObject(content);QJsonArray points=result.value("key_points").toArray();if(exactPointCount>0&&points.size()<exactPointCount){emit summaryFailed(QStringLiteral("总结结果没有满足你指定的 %1 条要求，请点击重新生成。").arg(exactPointCount));reply->deleteLater();return;}while(exactPointCount>0&&points.size()>exactPointCount)points.removeLast();if(exactPointCount>0)result.insert("key_points",points);if(result.value("overview").toString().trimmed().isEmpty()&&points.isEmpty())emit summaryFailed(QStringLiteral("总结魔法没有得到有效正文，请稍后重试。"));else emit summaryCompleted(result);reply->deleteLater();});
}

void AiService::generateDream(const QJsonObject &dreamContext)
{
    if(!isConfigured()){emit dreamFailed(QStringLiteral("AI尚未配置，今晚的梦还没有凝结成星星纸。"));return;}
    if(m_busy){emit dreamFailed(QStringLiteral("精灵正在处理其他事情，梦境稍后再试。"));return;}
    const QString system=QStringLiteral(
        "你是情绪精灵 Stellacandie 的独立梦境生成逻辑角色。梦是精灵自己的主观体验，不是任务、签到或购物清单。"
        "根据提供的近期真实片段、长期记忆摘要、精灵状态、季节和随机主题创作一则第一人称梦境。允许象征、混合与荒诞变形，"
        "但不得把梦中的虚构事件说成现实发生过；不得催促用户查看、回应、消费或照顾精灵；不得情感勒索。消费型内容只能作为偶然背景，"
        "不能表达索取。没有足够素材时可生成纯幻想或模糊梦。正文90到220字，童真、有具体画面，不过度鸡汤。"
        "只输出JSON对象：{\"title\":\"梦名\",\"content\":\"梦境正文\",\"mood\":\"warm|funny|quiet|strange|slightly_unsettled\","
        "\"dream_type\":\"memory_remix|long_memory|state_reflection|random_fantasy|fuzzy|continuation\","
        "\"symbols\":[\"意象，2至4个\"],\"color\":\"#RRGGBB\",\"reality_hint\":\"一句不带要求的现实联想，可为空\","
        "\"continuation_key\":\"连续梦线索或空字符串\",\"memory_ids\":[\"实际采用的候选记忆ID，只能从输入选择\"]}。"
        "禁止输出 opened、favorite、用户是否阅读等任何字段，也不要推测这些信息。" );
    QJsonArray messages{{QJsonObject{{"role","system"},{"content",system}}},QJsonObject{{"role","user"},{"content",QString::fromUtf8(QJsonDocument(dreamContext).toJson(QJsonDocument::Compact))}}};
    QJsonObject body{{"model",m_model},{"messages",messages},{"stream",false},{"max_tokens",1100},{"temperature",0.82},{"response_format",QJsonObject{{"type","json_object"}}},{"thinking",QJsonObject{{"type","disabled"}}}};
    setBusy(true);QNetworkReply*reply=m_network.post(makeRequest(QStringLiteral("/chat/completions")),QJsonDocument(body).toJson(QJsonDocument::Compact));m_activeReply=reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply]{m_activeReply.clear();setBusy(false);const QByteArray payload=reply->readAll();if(reply->error()!=QNetworkReply::NoError){emit dreamFailed(friendlyNetworkError(reply));reply->deleteLater();return;}const auto choices=QJsonDocument::fromJson(payload).object().value("choices").toArray();const QString content=choices.isEmpty()?QString():textFromJsonValue(choices.first().toObject().value("message").toObject().value("content"));const QJsonObject result=parseLooseJsonObject(content);if(result.value("title").toString().trimmed().isEmpty()||result.value("content").toString().trimmed().isEmpty())emit dreamFailed(QStringLiteral("这次只记住了一团模糊的光，没有形成可以折起来的梦。"));else emit dreamCompleted(result);reply->deleteLater();});
}

void AiService::sendConnectionTest(int attempt)
{
    QNetworkReply *reply = m_network.get(makeRequest(QStringLiteral("/user/balance")));
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, attempt] {
        m_activeReply.clear();
        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            if (attempt < 2 && shouldRetry(reply)) {
                reply->deleteLater();
                m_network.clearConnectionCache();
                m_network.clearAccessCache();
                emit statusMessage(QStringLiteral("连接暂时中断，正在自动重连（%1/2）…").arg(attempt + 1));
                QTimer::singleShot(800 * (attempt + 1), this, [this, attempt] {
                    sendConnectionTest(attempt + 1);
                });
                return;
            }
            setBusy(false);
            emit connectionTestFinished(false, friendlyNetworkError(reply));
            reply->deleteLater();
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(payload).object();
        const bool available = root.value(QStringLiteral("is_available")).toBool();
        QString balanceText;
        const QJsonArray balances = root.value(QStringLiteral("balance_infos")).toArray();
        for (const QJsonValue &value : balances) {
            const QJsonObject balance = value.toObject();
            balanceText += QStringLiteral("%1 %2 ")
                               .arg(balance.value(QStringLiteral("total_balance")).toString(),
                                    balance.value(QStringLiteral("currency")).toString());
        }
        setBusy(false);
        emit connectionTestFinished(available,
            available ? QStringLiteral("连接成功，可用余额：%1").arg(balanceText.trimmed())
                      : QStringLiteral("连接成功，但当前账户没有可用余额。"));
        reply->deleteLater();
    });
}

void AiService::cancel()
{
    if (m_activeReply) {
        m_activeReply->abort();
    }
}

QNetworkRequest AiService::makeRequest(const QString &path) const
{
    QNetworkRequest request(QUrl(m_baseUrl + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_apiKey.toUtf8());
    request.setTransferTimeout(45000);
    return request;
}

QString AiService::loadSystemPrompt() const
{
    QFile file(QStringLiteral(":/assets/prompts/system_prompt.txt"));
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
        ? QString::fromUtf8(file.readAll()) : QStringLiteral("你是Stellacandie猫精灵。只输出JSON。");
}

QString AiService::friendlyNetworkError(QNetworkReply *reply) const
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401) return QStringLiteral("API Key无效，请在设置中重新填写。错误码401。");
    if (status == 402) return QStringLiteral("API余额不足，请充值后重试。错误码402。");
    if (status == 429) return QStringLiteral("请求过于频繁，请稍后再试。错误码429。");
    if (status >= 500) return QStringLiteral("AI服务暂时不可用，已切换为离线回复。错误码%1。").arg(status);
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return QStringLiteral("AI请求已取消。");
    return QStringLiteral("网络请求失败：%1").arg(reply->errorString());
}

bool AiService::shouldRetry(QNetworkReply *reply) const
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 400 || status == 401 || status == 402 || status == 403
        || status == 404 || status == 422) {
        return false;
    }
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return false;
    }
    return status == 0 || status == 408 || status == 429 || status >= 500;
}

void AiService::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged(busy);
}
