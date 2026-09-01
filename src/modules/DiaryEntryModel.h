#pragma once

#include "../data/repositories/DiaryRepository.h"
#include <QAbstractListModel>

class DiaryEntryModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { DateRole = Qt::UserRole + 1, PreviewRole, ContentRole, UpdatedRole };
    explicit DiaryEntryModel(DiaryRepository *repository, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void refresh();
    DiaryEntryRecord entryAt(int row) const;
private:
    DiaryRepository *m_repository;
    QList<DiaryEntryRecord> m_entries;
};
