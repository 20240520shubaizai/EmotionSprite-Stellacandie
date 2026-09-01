#pragma once

#include "FeatureModule.h"
#include "../data/repositories/CollectionRepository.h"
#include <QUrl>

class FileSnackModule final : public FeatureModule
{
    Q_OBJECT
public:
    explicit FileSnackModule(CollectionRepository *storage, QObject *parent = nullptr);
    QString id() const override;
    QString displayName() const override;
    bool isEnabled() const override;
    void setEnabled(bool enabled) override;

    QString sourcePath() const { return m_sourcePath; }
    QString fileName() const { return m_fileName; }
    QString fileInfo() const { return m_fileInfo; }
    QString modifiedText() const { return m_modifiedText; }
    QString snackName() const { return m_snackName; }
    QString snackEmoji() const { return m_snackEmoji; }
    QString warningText() const { return m_warningText; }
    QString safetyLevel() const { return m_safetyLevel; }
    QString refusalReason() const { return m_refusalReason; }
    bool strongConfirmationRequired() const { return m_strongConfirmationRequired; }
    bool hasPendingSnack() const { return !m_sourcePath.isEmpty(); }

    QStringList inventoryItems() const;
    QStringList catalogItems() const;
    QStringList historyItems() const;
    QStringList protectedDirectories() const;
    QStringList pendingFileItems() const;
    int pendingNutrition() const { return m_nutrition; }
    int inventoryNutrition(int row) const;
    qint64 inventoryId(int row)const;
    int rowForInventoryId(qint64 id)const;

    bool prepare(const QUrl &url);
    bool prepareMany(const QList<QUrl> &urls);
    bool storePending(QString *errorMessage = nullptr);
    bool consumePending(QString *errorMessage = nullptr);
    bool eatInventory(int row, QString *errorMessage = nullptr);
    bool protectPendingDirectory(QString *errorMessage = nullptr);
    void clear();

signals:
    void changed();
    void enabledChanged(bool enabled);
    void snackConsumed(const QString &snackName, const QString &snackEmoji, int nutrition, const QString &reaction);

private:
    bool movePendingToTrash(QString *errorMessage);
    QString reactionFor(const SnackCatalogRecord &record) const;
    void refreshData();

    CollectionRepository *m_storage = nullptr;
    bool m_enabled = true;
    QString m_sourcePath, m_fileName, m_fileInfo, m_modifiedText;
    QString m_snackType, m_snackName, m_snackEmoji, m_warningText, m_safetyLevel, m_refusalReason;
    qint64 m_sourceSize = 0;
    int m_nutrition = 0;
    bool m_strongConfirmationRequired = false;
    QList<SnackInventoryRecord> m_inventory;
    QList<SnackCatalogRecord> m_catalog;
    QStringList m_history;
    QStringList m_sourcePaths, m_sourceNames, m_sourceTypes;
    QList<qint64> m_sourceSizes;
};
