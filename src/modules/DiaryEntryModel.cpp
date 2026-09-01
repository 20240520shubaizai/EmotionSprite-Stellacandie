#include "DiaryEntryModel.h"

DiaryEntryModel::DiaryEntryModel(DiaryRepository *repository, QObject *parent)
    : QAbstractListModel(parent), m_repository(repository) {}

int DiaryEntryModel::rowCount(const QModelIndex &parent) const
{ return parent.isValid() ? 0 : m_entries.size(); }

QVariant DiaryEntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) return {};
    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case DateRole: return entry.entryDate.toString(QStringLiteral("yyyy年M月d日"));
    case PreviewRole: return entry.content.left(42).simplified();
    case ContentRole: return entry.content;
    case UpdatedRole: return entry.updatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    default: return {};
    }
}

QHash<int, QByteArray> DiaryEntryModel::roleNames() const
{ return {{DateRole,"entryDate"},{PreviewRole,"preview"},{ContentRole,"content"},{UpdatedRole,"updatedAt"}}; }

void DiaryEntryModel::refresh()
{ beginResetModel(); m_entries = m_repository->loadDiaryEntries(); endResetModel(); }

DiaryEntryRecord DiaryEntryModel::entryAt(int row) const
{ return row >= 0 && row < m_entries.size() ? m_entries.at(row) : DiaryEntryRecord{}; }
