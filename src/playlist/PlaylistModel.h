#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonValue>
#include <QUrl>
#include <QVector>
#include <vector>

#include "Fragment.h"

class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(double totalDuration READ totalDuration NOTIFY totalDurationChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool nameValid READ nameValid NOTIFY nameChanged)
    Q_PROPERTY(QString nameValidationMessage READ nameValidationMessage NOTIFY nameChanged)
    Q_PROPERTY(QString suggestedFileName READ suggestedFileName NOTIFY nameChanged)
    Q_PROPERTY(bool repeat READ repeat WRITE setRepeat NOTIFY repeatChanged)
    Q_PROPERTY(QDateTime createdAt READ createdAt NOTIFY timestampsChanged)
    Q_PROPERTY(QDateTime updatedAt READ updatedAt NOTIFY timestampsChanged)
    Q_PROPERTY(bool hasPlaylist READ hasPlaylist NOTIFY hasPlaylistChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY validityChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QUrl fileUrl READ fileUrl NOTIFY fileUrlChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)

public:
    enum Roles {
        SourceRole = Qt::UserRole + 1,
        FileNameRole,
        StartRole,
        EndRole,
        DurationRole,
        DelayBeforeRole,
        AudioEnabledRole,
        VolumeRole,
        SpeedRole,
        LabelRole,
        NotesRole,
        DelayColorRole,
        SourceStatusRole,
        ValidRole,
        ValidationMessageRole
    };

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const;
    double totalDuration() const;
    QString name() const;
    void setName(const QString &name);
    bool nameValid() const;
    QString nameValidationMessage() const;
    QString suggestedFileName() const;
    bool repeat() const;
    void setRepeat(bool repeat);
    QDateTime createdAt() const;
    QDateTime updatedAt() const;
    bool hasPlaylist() const;
    bool valid() const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool modified() const;
    bool canUndo() const;
    bool canRedo() const;
    QUrl fileUrl() const;
    const Fragment *fragmentAt(int row) const;

    Q_INVOKABLE void addFragment(const QUrl &source);
    Q_INVOKABLE int duplicateFragment(int row);
    Q_INVOKABLE void removeFragment(int row);
    Q_INVOKABLE void moveFragment(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void newPlaylist();
    Q_INVOKABLE bool load(const QUrl &fileUrl);
    Q_INVOKABLE bool save(const QUrl &fileUrl);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantList missingSources() const;
    Q_INVOKABLE void relinkSource(int row, const QUrl &source);
    Q_INVOKABLE int relinkMissingFromFolder(const QUrl &folderUrl);
    Q_INVOKABLE void updateFragment(int row, const QVariantMap &values);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool isValidName(const QString &name) const;

signals:
    void countChanged();
    void totalDurationChanged();
    void nameChanged();
    void repeatChanged();
    void timestampsChanged();
    void hasPlaylistChanged();
    void validityChanged();
    void modifiedChanged();
    void fileUrlChanged();
    void undoRedoChanged();
    void loadFailed(const QString &message);
    void saveFailed(const QString &message);

private:
    struct ModelState {
        std::vector<Fragment> items;
        QString name;
        bool repeat = false;
        QDateTime createdAt;
        QDateTime updatedAt;
        QUrl fileUrl;
    };

    void setModified(bool modified);
    void setFileUrl(const QUrl &fileUrl);
    void setTimestamps(const QDateTime &createdAt, const QDateTime &updatedAt);
    bool applyData(Fragment *fragment, const QVariant &value, int role);
    ModelState captureState() const;
    void restoreState(const ModelState &state);
    void pushUndoState();
    void clearUndoRedoHistory();
    void trimUndoHistory();
    static QString localPathFromUrl(const QUrl &fileUrl);
    static QString normalizedPlaylistName(const QString &name);
    static bool isValidPlaylistName(const QString &name);
    static QDateTime currentTimestamp();
    static QDateTime dateTimeFromJson(const QJsonValue &value);
    static QString dateTimeToJson(const QDateTime &dateTime);
    static bool sourceExists(const Fragment &fragment);
    static QString sourceStatus(const Fragment &fragment);
    static bool isValidFragment(const Fragment &fragment);
    static QString validationMessage(const Fragment &fragment);

    std::vector<Fragment> m_items;
    QString m_name;
    bool m_repeat = false;
    QDateTime m_createdAt;
    QDateTime m_updatedAt;
    bool m_hasPlaylist = false;
    bool m_modified = false;
    QUrl m_fileUrl;
    QVector<ModelState> m_undoStack;
    QVector<ModelState> m_redoStack;
};
