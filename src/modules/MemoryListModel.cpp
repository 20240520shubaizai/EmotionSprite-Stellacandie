#include "MemoryListModel.h"
QVariant MemoryListModel::data(const QModelIndex&i,int r)const{if(!i.isValid()||i.row()<0||i.row()>=items.size())return{};const auto&m=items.at(i.row());switch(r){case CategoryRole:return m.category;case SubjectRole:return m.subject;case ContentRole:return m.content;case ImportanceRole:return m.importance;case QuestionRole:return m.nextQuestion;default:return{};}}
QHash<int,QByteArray> MemoryListModel::roleNames()const{return{{CategoryRole,"category"},{SubjectRole,"subject"},{ContentRole,"memoryContent"},{ImportanceRole,"importance"},{QuestionRole,"nextQuestion"}};}
void MemoryListModel::refresh()
{
    if (!repo) return;
    // 先完整读取，再进入模型重置区；避免数据库读取期间模型处于 reset 状态，
    // 也绕开部分 Qt 版本下 QList 移动赋值造成的悬空数据问题。
    const QList<MemoryRecord> loaded = repo->loadMemories();
    beginResetModel();
    items.clear();
    items.reserve(loaded.size());
    for (const auto &record : loaded) items.append(record);
    endResetModel();
}
