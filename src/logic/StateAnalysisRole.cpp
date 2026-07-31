#include "StateAnalysisRole.h"

QString StateAnalysisRole::id() const { return QStringLiteral("state_analysis"); }

QString StateAnalysisRole::instruction() const
{
    return QStringLiteral(
        "你承担状态判定逻辑角色。state_effect只描述本条消息带来的额外语义影响；"
        "程序已固定执行每次说话精力-1和冷落-2，禁止重复计算。普通无感情色彩的信息全部填0。"
        "各字段必须是整数：mood[-6,6]、energy[-4,3]、closeness[-1,1]、boredom[-6,6]、"
        "curiosity[-6,6]、irritation[-6,6]、confidence[0,100]，并用reason简述依据。"
        "用户的负面情绪不等于精灵必须同幅度变差；应判断这件事对精灵自身的真实影响。"
        "例如‘我收拾一下准备去开会’是中性信息，所有额外影响均为0。"
        "开心且有细节的故事通常提高mood和curiosity并降低boredom；机械重复提高boredom；"
        "关心精灵可缓慢提高closeness；攻击、敷衍或反复失约可提高irritation。"
        "closeness单次只能变化-1、0或1。");
}
