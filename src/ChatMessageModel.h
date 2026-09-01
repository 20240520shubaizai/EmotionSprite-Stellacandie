#pragma once

#include "data/repositories/ConversationRepository.h"

#include <QAbstractListModel>

class ChatMessageModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        SenderRole = Qt::UserRole + 1,
        TextRole,
        TimestampRole,
        IsUserRole,
    };

    explicit ChatMessageModel(ConversationRepository *storage, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void load();
    void append(const QString &sender, const QString &text);
    void appendPersisted(const ChatMessageRecord &record);

private:
    ConversationRepository *m_storage = nullptr;
    QList<ChatMessageRecord> m_messages;
};
