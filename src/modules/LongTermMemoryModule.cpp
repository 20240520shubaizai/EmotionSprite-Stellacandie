#include "LongTermMemoryModule.h"
#include "../AiService.h"
#include <QJsonObject>
#include <QSettings>
#include <QRegularExpression>
#include <QDateTime>
#include <algorithm>

LongTermMemoryModule::LongTermMemoryModule(DataRepository *r,AiService *ai,QObject *p):FeatureModule(p),m_repository(r),m_ai(ai)
{
    m_enabled=QSettings().value("modules/longTermMemoryEnabled",true).toBool();
    connect(ai,&AiService::memoryAnalysisCompleted,this,[this](const QJsonArray &items){
        int saved=0; for(const auto &v:items){const auto o=v.toObject(); MemoryRecord m;
            m.category=o.value("category").toString();m.subject=o.value("subject").toString().trimmed();m.content=o.value("content").toString().trimmed();
            m.importance=std::clamp(o.value("importance").toInt(50),0,100);m.confidence=o.value("confidence").toDouble(.8);if(m.confidence>1)m.confidence/=100.0;m.nextQuestion=o.value("next_question").toString().trimmed();
            m.retention=o.value("retention").toString(QStringLiteral("long_term"));m.locked=o.value("locked").toBool(false);m.governanceReason=o.value("governance_reason").toString().trimmed();m.memoryState=QStringLiteral("active");
            if(m.retention==QStringLiteral("temporary"))m.expiresAt=QDateTime::currentDateTime().addDays(7);else if(m.retention==QStringLiteral("normal"))m.expiresAt=QDateTime::currentDateTime().addDays(180);else if(m.retention==QStringLiteral("permanent"))m.locked=true;
            const QRegularExpression secretRe(QStringLiteral("(?:api[_ -]?key|密码|口令|验证码|银行卡|身份证|私钥|secret|token)"),QRegularExpression::CaseInsensitiveOption);
            if(secretRe.match(m.subject+QLatin1Char(' ')+m.content).hasMatch())continue;
            const QRegularExpression timeBoundRe(QStringLiteral("(?:提醒我|通知我|叫我|今天|明天|后天|\\d+天后|\\d{1,2}[点时]|开会|会议|面试|考试|待办)"));
            if((m.category==QStringLiteral("event")||m.category==QStringLiteral("story"))&&timeBoundRe.match(m.subject+QLatin1Char(' ')+m.content).hasMatch())continue;
            if(m.importance>=90)m.locked=true;
            for(const auto &old:m_repository->loadMemories())if((m.category=="event"||m.category=="story")&&(old.category=="event"||old.category=="story")
                &&!old.subject.isEmpty()&&m.content.contains(old.subject)){m.category=old.category;m.subject=old.subject;break;}
            bool blocked=false;for(const QString &forgotten:m_repository->forgottenTopics())if(m.subject.contains(forgotten)||m.content.contains(forgotten)){blocked=true;break;}
            if(!blocked&&!m.category.isEmpty()&&!m.subject.isEmpty()&&!m.content.isEmpty()&&m.confidence>=.55&&m_repository->upsertMemory(m)){
                saved++;for(const QString &entity:m_repository->entityNames())if(m.subject.contains(entity)||m.content.contains(entity))m_repository->linkMemoryToEntity(m.category,m.subject,entity);
            }
        }
        auto all=m_repository->loadMemories();for(int i=0;i<all.size();++i)for(int j=i+1;j<all.size();++j)
            if((all[i].category=="story"||all[i].category=="event")&&(all[j].category=="story"||all[j].category=="event")
                &&(all[i].content.contains(all[j].subject)||all[j].content.contains(all[i].subject)))
                m_repository->softDeleteMemory(all[i].importance>=all[j].importance?all[j].id:all[i].id);
        emit memoriesChanged(); emit analysisStatus(QStringLiteral("本轮整理了%1条长期记忆").arg(saved));
    });
    connect(ai,&AiService::memoryAnalysisFailed,this,[this](const QString &e){emit analysisStatus(QStringLiteral("记忆整理暂缓：%1").arg(e));});
}
QString LongTermMemoryModule::id()const{return "long_term_memory";} QString LongTermMemoryModule::displayName()const{return QStringLiteral("长期记忆");}
bool LongTermMemoryModule::isEnabled()const{return m_enabled;} void LongTermMemoryModule::setEnabled(bool e){if(e==m_enabled)return;m_enabled=e;QSettings().setValue("modules/longTermMemoryEnabled",e);emit enabledChanged(e);}
void LongTermMemoryModule::analyzeRecentConversation()
{
    if(!m_enabled)return;
    const auto recent=m_repository->loadRecentMessages(6);
    QString lastUser; for(auto it=recent.crbegin();it!=recent.crend();++it)if(it->sender==QStringLiteral("user")){lastUser=it->text;break;}
    const QRegularExpression transactionRe(QStringLiteral("(?:提醒我|通知我|叫我|别让我忘|今天|明天|后天|\\d+天后|\\d{1,2}[点时].*(?:开会|会议|面试|考试|换|买|做))"));
    if(transactionRe.match(lastUser).hasMatch())return;
    if(lastUser.size()>=8){
        QRegularExpression personRe(QStringLiteral("(?:朋友|同事|同学)(?:叫|是)?([\\p{Han}]{1,4}?)(?=下周|明天|后天|要|会|从|，|。)"));
        auto match=personRe.match(lastUser); MemoryRecord local;
        if(match.hasMatch()){local.category=QStringLiteral("story");local.subject=match.captured(1);local.content=lastUser;
            local.importance=75;local.confidence=.92;local.nextQuestion=QStringLiteral("%1后来怎么样了？").arg(local.subject);
            m_repository->ensureEntity(local.subject,QStringLiteral("person"));
            if(m_repository->upsertMemory(local)){m_repository->linkMemoryToEntity(local.category,local.subject,local.subject);emit memoriesChanged();}}
        QRegularExpression prefRe(QStringLiteral("我(喜欢|讨厌)([^，。！？]{1,12})")); auto pref=prefRe.match(lastUser);
        if(pref.hasMatch()){local.category=QStringLiteral("preference");local.subject=pref.captured(2).trimmed();local.content=lastUser;
            local.importance=65;local.confidence=.9;local.nextQuestion.clear();m_repository->upsertMemory(local);emit memoriesChanged();}
    }
    m_ai->analyzeMemories(recent);
}

QString LongTermMemoryModule::relevantContext(const QString &text)
{
    if(!m_enabled)return {}; auto memories=m_repository->loadMemories();
    std::stable_sort(memories.begin(),memories.end(),[&](const auto&a,const auto&b){
        auto score=[&](const auto&m){int s=m.importance; if(text.contains(m.subject,Qt::CaseInsensitive))s+=100;
            for(const QString &w:text.split(QRegularExpression("[，。！？、\\s]+"),Qt::SkipEmptyParts))if(w.size()>=2&&m.content.contains(w))s+=20; if(!m.nextQuestion.isEmpty())s+=8; return s;};return score(a)>score(b);});
    QStringList lines;for(const auto&m:memories){bool related=text.contains(m.subject,Qt::CaseInsensitive);
        for(const QString&w:text.split(QRegularExpression("[，。！？、\\s]+"),Qt::SkipEmptyParts))if(w.size()>=2&&m.content.contains(w))related=true;
        if(!related&&(text.contains("周末")||text.contains("后来")||text.contains("忙完"))&&!m.nextQuestion.isEmpty())related=true;
        if(!related)continue;QString marker=(m.content.contains("去世")||m.content.contains("已故")||m.content.contains("死亡"))
            ?QStringLiteral("[SENSITIVE_MEMORY subject=%1] ").arg(m.subject):QString();
        lines<<QStringLiteral("%1- [%2 confidence=%3] %4").arg(marker,m.category).arg(m.confidence,0,'f',2).arg(m.content);m_repository->touchMemory(m.id);if(lines.size()>=4)break;}
    // “记得提醒我”是创建未来提醒，不是在询问过去的记忆，不能触发“不记得”兜底。
    const QRegularExpression recallQuestion(QStringLiteral("(?:你还记得|还记得.*吗|记不记得|你想起|想得起来)"));
    if(lines.isEmpty()&&recallQuestion.match(text).hasMatch()&&!text.contains(QStringLiteral("提醒我")))
        return QStringLiteral("[NO_RELEVANT_MEMORY] 系统没有找到与当前问题相关的已确认记忆。必须诚实说不记得，禁止猜测。");
    return lines.isEmpty()?QString():QStringLiteral("[MEMORY_CONTEXT] 可参考的长期记忆（只在自然相关时提起，不要逐条复述）：\n")+lines.join('\n');
}
QString LongTermMemoryModule::personalityContext(int closeness,int boredom)const
{
    return QStringLiteral("人格连续性：你始终是好奇、略有小脾气、有自己感受的猫精灵，不是情绪垃圾桶。"
        "亲密阶段=%1；当前无聊度=%2。机械寒暄过多时可以坦率说无聊并追问新鲜细节；"
        "不要一味安慰，也不要因一次谈话改变核心性格。记忆不确定时要用疑问语气确认。")
        .arg(closeness<35?QStringLiteral("熟悉中"):closeness<70?QStringLiteral("亲近"):QStringLiteral("深度信任")).arg(boredom);
}

QString LongTermMemoryModule::offlineRecallReply(const QString &message) const
{
    if(!m_enabled)return{};const auto all=m_repository->loadMemories();
    for(const auto&m:all)if(message.contains(m.subject,Qt::CaseInsensitive)||message.contains(QStringLiteral("还记得"))){
        QString reply=QStringLiteral("当然记得呀——%1").arg(m.content);if(!m.nextQuestion.isEmpty())reply+=QStringLiteral(" ")+m.nextQuestion;return reply;}
    return{};
}

bool LongTermMemoryModule::handleUserDirective(const QString &message, QString *reply)
{
    if(!m_enabled)return false;
    if(message.contains(QStringLiteral("忘掉"))||message.contains(QStringLiteral("忘记"))||message.contains(QStringLiteral("别再提"))){
        QString topic;for(const auto&m:m_repository->loadMemories())if(message.contains(m.subject)){topic=m.subject;break;}
        if(topic.isEmpty())for(const QString&e:m_repository->entityNames())if(message.contains(e)){topic=e;break;}
        if(topic.isEmpty()){
            QRegularExpression re(QStringLiteral("(?:忘掉|忘记|别再提)(?:我和|关于|那个|这段|这件)?([^，。！？]{1,12}?)(?:的事情|这件事|以后|吧|，|。|！|$)"));auto match=re.match(message);if(match.hasMatch())topic=match.captured(1).trimmed();
        }
        if(topic.isEmpty())topic=QStringLiteral("这件事");const int count=m_repository->forgetTopic(topic);emit memoriesChanged();
        if(reply)*reply=count>0?QStringLiteral("好，我把关于“%1”的记忆和后续提醒都收起来了，以后不会主动提。").arg(topic)
            :QStringLiteral("好，我记下了：不再保存或主动提起“%1”。").arg(topic);return true;
    }
    return applyFactCorrection(message,reply);
}

bool LongTermMemoryModule::applyFactCorrection(const QString &message, QString *reply)
{
    QRegularExpression re(QStringLiteral("不是([^，。！？来去]{1,10})(?:来|去)?.*?(?:改成|改为)([^，。！？了]{1,10})"));auto match=re.match(message);if(!match.hasMatch())return false;
    const QString oldValue=match.captured(1).trimmed(),newValue=match.captured(2).trimmed();QString entity;
    for(const QString&e:m_repository->entityNames())if(message.contains(e)){entity=e;break;}
    if(entity.isEmpty())for(const auto&m:m_repository->loadMemories())if(message.contains(m.subject)){entity=m.subject;break;}
    int changed=0;for(const auto&m:m_repository->loadMemories())if((entity.isEmpty()||m.subject.contains(entity)||m.content.contains(entity))&&m.content.contains(oldValue)){
        QString content=m.content;content.replace(oldValue,newValue);QString question=m.nextQuestion;question.replace(oldValue,newValue);
        if(m_repository->updateMemoryContent(m.id,content,question))changed++;
    }
    if(changed==0)return false;emit memoriesChanged();if(reply)*reply=QStringLiteral("哦，是%1，不是%2。我已经改好了——这次不会再拿旧时间说事。").arg(newValue,oldValue);return true;
}
