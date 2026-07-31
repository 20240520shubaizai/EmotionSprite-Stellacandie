#pragma once

#include "FeatureModule.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <functional>

class MemeCultureModule final : public FeatureModule
{
    Q_OBJECT
public:
    explicit MemeCultureModule(QObject *parent = nullptr);
    QString id() const override;
    QString displayName() const override;
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;
    QString contextForMessage(const QString &message);
    void enrichContext(const QString &message, const QString &baseContext, std::function<void(QString)> done);
    void recordAssistantReply(const QString &reply);

    // Returns true when this message belongs to the learning dialogue and normal AI dispatch must stop.
    bool handleLearningMessage(const QString &message, std::function<void(QString)> reply);
    QStringList learnedMemeSummaries() const;
    bool removeLearnedMeme(int index);

signals:
    void learnedMemesChanged();

private:
    bool fresh(const QJsonObject &entry) const;
    bool containsMeme(const QString &reply) const;
    int recentCount() const;
    QString searchKeyword(const QString &message) const;
    QString inferMemeTerm(const QString &message, const QString &previous) const;
    void searchMeme(const QString &keyword, std::function<void(QString)> done);
    void saveLearnedMeme(const QString &term, const QString &webSummary,
                         const QString &userView, bool shared);
    void loadLearnedMemes();
    void persistLearnedMemes();

    QJsonArray m_entries;
    QJsonArray m_learnedEntries;
    QNetworkAccessManager m_network;
    QString m_lastUserMessage;
    QString m_pendingTerm;
    QString m_pendingWebSummary;
    bool m_enabled = true;
};
