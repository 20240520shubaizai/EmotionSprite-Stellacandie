#include "HealthCareRole.h"
QString HealthCareRole::id()const{return QStringLiteral("health_care");}
QString HealthCareRole::instruction()const{return QStringLiteral(
    "仅当HEALTH_CONTEXT显示精灵不是healthy时判断本条消息的照料效果，并在顶层返回care_effect对象："
    "care_type只能是none、normal_chat、happy_story、detailed_story、comfort、rest；care_recovery必须为0到6整数。"
    "普通聊天0到1，开心故事2到4，新鲜且有细节的开心故事4到6，关心精灵1到2，让精灵休息3。"
    "用户倾诉负面情绪绝不扣健康、不倒扣恢复，也不要求用户负责。healthy时care_recovery固定0。");}
