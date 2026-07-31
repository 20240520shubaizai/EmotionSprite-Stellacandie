#include "ChatMessageModel.h"

ChatMessageModel::ChatMessageModel(DataRepository *storage, QObject *parent)
    : QAbstractListModel(parent)
    , m_storage(storage)
{
}

int ChatMessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_messages.size());
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return {};
    }

    const ChatMessageRecord &message = m_messages.at(index.row());
    switch (role) {
    case SenderRole:
        return message.sender;
    case TextRole:
        return message.text;
    case TimestampRole:
        return message.createdAt.toString(QStringLiteral("HH:mm"));
    case IsUserRole:
        return message.sender == QStringLiteral("user");
    default:
        return {};
    }
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const
{
    return {{SenderRole, "sender"},
            {TextRole, "messageText"},
            {TimestampRole, "timestamp"},
            {IsUserRole, "isUser"}};
}

void ChatMessageModel::load()
{
    beginResetModel();
    m_messages = m_storage->loadRecentMessages();
    endResetModel();
}

void ChatMessageModel::append(const QString &sender, const QString &text)
{
    const ChatMessageRecord record = m_storage->addMessage(sender, text);
    const int row = m_messages.size();
    beginInsertRows(QModelIndex(), row, row);
    m_messages.append(record);
    endInsertRows();
}
