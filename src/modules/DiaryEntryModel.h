#pragma once

#include "../data/DataRepository.h"
#include <QAbstractListModel>

class DiaryEntryModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { DateRole = Qt::UserRole + 1, PreviewRole, ContentRole, UpdatedRole };
    explicit DiaryEntryModel(DataRepository *repository, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void refresh();
    DiaryEntryRecord entryAt(int row) const;
private:
    DataRepository *m_repository;
    QList<DiaryEntryRecord> m_entries;
};

