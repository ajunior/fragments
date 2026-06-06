#include "PlaylistModel.h"

#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace {
QString normalizedColor(const QString &value, const QString &fallback = QStringLiteral("#000000"))
{
    QString normalized = value.trimmed();
    static const QRegularExpression hexColor(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    static const QRegularExpression hexColorWithAlpha(QStringLiteral("^#[0-9a-fA-F]{8}$"));
    if (hexColorWithAlpha.match(normalized).hasMatch()) {
        normalized = QStringLiteral("#") + normalized.mid(3);
    } else if (!hexColor.match(normalized).hasMatch()) {
        return fallback;
    }

    return normalized.toLower();
}

double mediaDurationSeconds(const QString &path)
{
    const QString ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty()) {
        return -1.0;
    }

    QProcess probe;
    probe.start(ffprobe, {
        QStringLiteral("-v"),
        QStringLiteral("error"),
        QStringLiteral("-show_entries"),
        QStringLiteral("format=duration"),
        QStringLiteral("-of"),
        QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        path
    });

    if (!probe.waitForStarted(3000) || !probe.waitForFinished(10000) || probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        return -1.0;
    }

    bool ok = false;
    const double duration = QString::fromUtf8(probe.readAllStandardOutput()).trimmed().toDouble(&ok);
    return ok && duration > 0.0 ? duration : -1.0;
}

QString jsonTypeName(const QJsonValue &value)
{
    if (value.isNull()) {
        return QStringLiteral("null");
    }
    if (value.isBool()) {
        return QStringLiteral("boolean");
    }
    if (value.isDouble()) {
        return QStringLiteral("number");
    }
    if (value.isString()) {
        return QStringLiteral("string");
    }
    if (value.isArray()) {
        return QStringLiteral("array");
    }
    if (value.isObject()) {
        return QStringLiteral("object");
    }
    return QStringLiteral("undefined");
}

bool hasOnlyProperties(const QJsonObject &object, const QStringList &allowed, QString *message, const QString &context)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            *message = QStringLiteral("%1 has unknown property '%2'.").arg(context, it.key());
            return false;
        }
    }
    return true;
}

bool requireProperty(const QJsonObject &object, const QString &key, QString *message, const QString &context)
{
    if (!object.contains(key)) {
        *message = QStringLiteral("%1 is missing required property '%2'.").arg(context, key);
        return false;
    }
    return true;
}

bool requireString(const QJsonObject &object, const QString &key, QString *message, const QString &context)
{
    if (!requireProperty(object, key, message, context)) {
        return false;
    }
    if (!object.value(key).isString()) {
        *message = QStringLiteral("%1.%2 must be a string, got %3.").arg(context, key, jsonTypeName(object.value(key)));
        return false;
    }
    return true;
}

bool requireNumber(const QJsonObject &object, const QString &key, double minimum, double maximum, QString *message, const QString &context)
{
    if (!requireProperty(object, key, message, context)) {
        return false;
    }

    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        *message = QStringLiteral("%1.%2 must be a number, got %3.").arg(context, key, jsonTypeName(value));
        return false;
    }

    const double number = value.toDouble();
    if (number < minimum || number > maximum) {
        *message = QStringLiteral("%1.%2 must be between %3 and %4.").arg(context, key).arg(minimum).arg(maximum);
        return false;
    }
    return true;
}

bool requireNonNegativeNumber(const QJsonObject &object, const QString &key, QString *message, const QString &context)
{
    if (!requireProperty(object, key, message, context)) {
        return false;
    }

    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        *message = QStringLiteral("%1.%2 must be a number, got %3.").arg(context, key, jsonTypeName(value));
        return false;
    }
    if (value.toDouble() < 0.0) {
        *message = QStringLiteral("%1.%2 must be 0 or greater.").arg(context, key);
        return false;
    }
    return true;
}

bool validatePlaylistJson(const QJsonObject &root, QString *message)
{
    const QString rootContext = QStringLiteral("Playlist");
    const QStringList rootProperties {
        QStringLiteral("version"),
        QStringLiteral("name"),
        QStringLiteral("repeat"),
        QStringLiteral("createdAt"),
        QStringLiteral("updatedAt"),
        QStringLiteral("items")
    };
    if (!hasOnlyProperties(root, rootProperties, message, rootContext)) {
        return false;
    }

    if (!requireProperty(root, QStringLiteral("version"), message, rootContext)) {
        return false;
    }
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toInt(-1) != 1 || version.toDouble() != 1.0) {
        *message = QStringLiteral("Playlist.version must be 1.");
        return false;
    }

    if (!requireString(root, QStringLiteral("name"), message, rootContext)) {
        return false;
    }
    static const QRegularExpression namePattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_-]*$"));
    if (!namePattern.match(root.value(QStringLiteral("name")).toString()).hasMatch()) {
        *message = QStringLiteral("Playlist.name must start with a letter or _ and contain only letters, numbers, _ or -.");
        return false;
    }

    if (!requireProperty(root, QStringLiteral("repeat"), message, rootContext)) {
        return false;
    }
    if (!root.value(QStringLiteral("repeat")).isBool()) {
        *message = QStringLiteral("Playlist.repeat must be a boolean, got %1.").arg(jsonTypeName(root.value(QStringLiteral("repeat"))));
        return false;
    }

    for (const QString &key : { QStringLiteral("createdAt"), QStringLiteral("updatedAt") }) {
        if (!requireString(root, key, message, rootContext)) {
            return false;
        }
        if (!QDateTime::fromString(root.value(key).toString(), Qt::ISODateWithMs).isValid()
            && !QDateTime::fromString(root.value(key).toString(), Qt::ISODate).isValid()) {
            *message = QStringLiteral("Playlist.%1 must be a valid ISO date-time.").arg(key);
            return false;
        }
    }

    if (!requireProperty(root, QStringLiteral("items"), message, rootContext)) {
        return false;
    }
    if (!root.value(QStringLiteral("items")).isArray()) {
        *message = QStringLiteral("Playlist.items must be an array, got %1.").arg(jsonTypeName(root.value(QStringLiteral("items"))));
        return false;
    }

    const QStringList fragmentProperties {
        QStringLiteral("file"),
        QStringLiteral("start"),
        QStringLiteral("end"),
        QStringLiteral("delayBefore"),
        QStringLiteral("audio"),
        QStringLiteral("volume"),
        QStringLiteral("speed"),
        QStringLiteral("label"),
        QStringLiteral("notes"),
        QStringLiteral("delayColor")
    };
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QString context = QStringLiteral("Playlist.items[%1]").arg(i);
        if (!items.at(i).isObject()) {
            *message = QStringLiteral("%1 must be an object, got %2.").arg(context, jsonTypeName(items.at(i)));
            return false;
        }

        const QJsonObject fragment = items.at(i).toObject();
        if (!hasOnlyProperties(fragment, fragmentProperties, message, context)) {
            return false;
        }

        if (!requireString(fragment, QStringLiteral("file"), message, context)) {
            return false;
        }
        const QUrl source(fragment.value(QStringLiteral("file")).toString());
        if (!source.isValid() || source.scheme().isEmpty()) {
            *message = QStringLiteral("%1.file must be an absolute URL.").arg(context);
            return false;
        }

        if (!requireNonNegativeNumber(fragment, QStringLiteral("start"), message, context)
            || !requireNonNegativeNumber(fragment, QStringLiteral("end"), message, context)
            || !requireNonNegativeNumber(fragment, QStringLiteral("delayBefore"), message, context)
            || !requireNumber(fragment, QStringLiteral("volume"), 0.0, 1.0, message, context)
            || !requireNumber(fragment, QStringLiteral("speed"), 0.1, 4.0, message, context)
            || !requireString(fragment, QStringLiteral("label"), message, context)
            || !requireString(fragment, QStringLiteral("notes"), message, context)
            || !requireString(fragment, QStringLiteral("delayColor"), message, context)) {
            return false;
        }

        if (!requireProperty(fragment, QStringLiteral("audio"), message, context)) {
            return false;
        }
        if (!fragment.value(QStringLiteral("audio")).isBool()) {
            *message = QStringLiteral("%1.audio must be a boolean, got %2.").arg(context, jsonTypeName(fragment.value(QStringLiteral("audio"))));
            return false;
        }

        static const QRegularExpression colorPattern(QStringLiteral("^#[0-9a-f]{6}$"));
        if (!colorPattern.match(fragment.value(QStringLiteral("delayColor")).toString()).hasMatch()) {
            *message = QStringLiteral("%1.delayColor must use lowercase #rrggbb format.").arg(context);
            return false;
        }
    }

    return true;
}
}

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_createdAt()
    , m_updatedAt()
{
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_items.size());
}

int PlaylistModel::count() const
{
    return rowCount();
}

double PlaylistModel::totalDuration() const
{
    double total = 0.0;
    for (const Fragment &fragment : m_items)
        total += fragment.end - fragment.start;
    return total;
}

QString PlaylistModel::name() const
{
    return m_name;
}

void PlaylistModel::setName(const QString &name)
{
    const QString trimmedName = normalizedPlaylistName(name);
    if (!trimmedName.isEmpty() && !isValidPlaylistName(trimmedName)) {
        return;
    }

    if (m_name == trimmedName) {
        return;
    }

    pushUndoState();
    m_name = trimmedName;
    emit nameChanged();
    setModified(true);
}

bool PlaylistModel::nameValid() const
{
    return isValidPlaylistName(m_name);
}

QString PlaylistModel::nameValidationMessage() const
{
    if (nameValid()) {
        return {};
    }

    return QStringLiteral("Use letters, numbers, _ or -. The name must start with a letter or _.");
}

QString PlaylistModel::suggestedFileName() const
{
    if (!nameValid()) {
        return {};
    }

    QString fileName = m_name.toLower();
    fileName.replace(QLatin1Char('-'), QLatin1Char('_'));
    return fileName + QStringLiteral(".json");
}

bool PlaylistModel::repeat() const
{
    return m_repeat;
}

void PlaylistModel::setRepeat(bool repeat)
{
    if (m_repeat == repeat) {
        return;
    }

    pushUndoState();
    m_repeat = repeat;
    emit repeatChanged();
    setModified(true);
}

QDateTime PlaylistModel::createdAt() const
{
    return m_createdAt;
}

QDateTime PlaylistModel::updatedAt() const
{
    return m_updatedAt;
}

bool PlaylistModel::hasPlaylist() const
{
    return m_hasPlaylist;
}

bool PlaylistModel::valid() const
{
    for (const Fragment &fragment : m_items) {
        if (!isValidFragment(fragment)) {
            return false;
        }
    }

    return true;
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const Fragment &fragment = m_items.at(static_cast<size_t>(index.row()));
    switch (role) {
    case SourceRole:
        return fragment.source;
    case FileNameRole:
        return fragment.source.isLocalFile() ? QFileInfo(fragment.source.toLocalFile()).fileName() : fragment.source.fileName();
    case StartRole:
        return fragment.start;
    case EndRole:
        return fragment.end;
    case DurationRole:
        return qMax(0.0, fragment.end - fragment.start);
    case DelayBeforeRole:
        return fragment.delayBefore;
    case AudioEnabledRole:
        return fragment.audioEnabled;
    case VolumeRole:
        return fragment.volume;
    case SpeedRole:
        return fragment.speed;
    case LabelRole:
        return fragment.label;
    case NotesRole:
        return fragment.notes;
    case DelayColorRole:
        return fragment.delayColor;
    case SourceStatusRole:
        return sourceStatus(fragment);
    case ValidRole:
        return isValidFragment(fragment);
    case ValidationMessageRole:
        return validationMessage(fragment);
    default:
        return {};
    }
}

bool PlaylistModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return false;
    }

    Fragment &fragment = m_items[static_cast<size_t>(index.row())];
    Fragment changed = fragment;
    if (!applyData(&changed, value, role)) {
        return false;
    }
    if (changed.source == fragment.source
        && changed.start == fragment.start
        && changed.end == fragment.end
        && changed.delayBefore == fragment.delayBefore
        && changed.audioEnabled == fragment.audioEnabled
        && changed.volume == fragment.volume
        && changed.speed == fragment.speed
        && changed.label == fragment.label
        && changed.notes == fragment.notes
        && changed.delayColor == fragment.delayColor) {
        return false;
    }

    pushUndoState();
    fragment = changed;

    emit dataChanged(index, index, {role, DurationRole, SourceStatusRole, ValidRole, ValidationMessageRole});
    emit totalDurationChanged();
    emit validityChanged();
    setModified(true);
    return true;
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {
        {SourceRole, "source"},
        {FileNameRole, "fileName"},
        {StartRole, "start"},
        {EndRole, "end"},
        {DurationRole, "duration"},
        {DelayBeforeRole, "delayBefore"},
        {AudioEnabledRole, "audioEnabled"},
        {VolumeRole, "volume"},
        {SpeedRole, "speed"},
        {LabelRole, "label"},
        {NotesRole, "notes"},
        {DelayColorRole, "delayColor"},
        {SourceStatusRole, "sourceStatus"},
        {ValidRole, "valid"},
        {ValidationMessageRole, "validationMessage"}
    };
}

bool PlaylistModel::modified() const
{
    return m_modified;
}

bool PlaylistModel::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool PlaylistModel::canRedo() const
{
    return !m_redoStack.isEmpty();
}

QUrl PlaylistModel::fileUrl() const
{
    return m_fileUrl;
}

const Fragment *PlaylistModel::fragmentAt(int row) const
{
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }

    return &m_items.at(static_cast<size_t>(row));
}

void PlaylistModel::addFragment(const QUrl &source)
{
    if (!source.isValid()) {
        return;
    }

    pushUndoState();
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    Fragment fragment;
    fragment.source = source;
    if (source.isLocalFile()) {
        const double duration = mediaDurationSeconds(source.toLocalFile());
        fragment.end = duration > 0.0 ? duration : fragment.end;
    }
    m_items.push_back(fragment);
    endInsertRows();
    emit countChanged();
    emit totalDurationChanged();
    emit validityChanged();
    setModified(true);
}

int PlaylistModel::duplicateFragment(int row)
{
    if (row < 0 || row >= rowCount()) {
        return -1;
    }

    const int insertRow = row + 1;
    pushUndoState();
    beginInsertRows(QModelIndex(), insertRow, insertRow);
    m_items.insert(m_items.begin() + insertRow, m_items.at(static_cast<size_t>(row)));
    endInsertRows();
    emit countChanged();
    emit totalDurationChanged();
    emit validityChanged();
    setModified(true);
    return insertRow;
}

void PlaylistModel::removeFragment(int row)
{
    if (row < 0 || row >= rowCount()) {
        return;
    }

    pushUndoState();
    beginRemoveRows(QModelIndex(), row, row);
    m_items.erase(m_items.begin() + row);
    endRemoveRows();
    emit countChanged();
    emit totalDurationChanged();
    emit validityChanged();
    setModified(true);
}

void PlaylistModel::moveFragment(int from, int to)
{
    if (from < 0 || from >= rowCount() || to < 0 || to >= rowCount() || from == to) {
        return;
    }

    const int destination = from < to ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destination)) {
        return;
    }

    pushUndoState();
    Fragment moved = m_items.at(static_cast<size_t>(from));
    m_items.erase(m_items.begin() + from);
    m_items.insert(m_items.begin() + to, moved);
    endMoveRows();
    emit validityChanged();
    setModified(true);
}

void PlaylistModel::clear()
{
    if (m_items.empty()) {
        return;
    }

    pushUndoState();
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
    emit totalDurationChanged();
    emit validityChanged();
    setModified(true);
}

void PlaylistModel::newPlaylist()
{
    const bool previousHasPlaylist = m_hasPlaylist;
    m_hasPlaylist = true;
    if (previousHasPlaylist != m_hasPlaylist) {
        emit hasPlaylistChanged();
    }

    clearUndoRedoHistory();
    beginResetModel();
    m_items.clear();
    endResetModel();
    const QString defaultName = QStringLiteral("Untitled");
    if (m_name != defaultName) {
        m_name = defaultName;
        emit nameChanged();
    }
    if (m_repeat) {
        m_repeat = false;
        emit repeatChanged();
    }
    setTimestamps(QDateTime(), QDateTime());
    emit countChanged();
    emit totalDurationChanged();
    emit validityChanged();
    setFileUrl(QUrl());
    setModified(false);
}

bool PlaylistModel::load(const QUrl &fileUrl)
{
    const QString path = localPathFromUrl(fileUrl);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit loadFailed(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit loadFailed(parseError.error != QJsonParseError::NoError ? parseError.errorString() : QStringLiteral("Playlist root must be an object."));
        return false;
    }

    const QJsonObject root = document.object();
    QString validationError;
    if (!validatePlaylistJson(root, &validationError)) {
        emit loadFailed(validationError);
        return false;
    }

    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    std::vector<Fragment> loaded;
    loaded.reserve(static_cast<size_t>(items.size()));

    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        Fragment fragment;
        fragment.source = QUrl(object.value(QStringLiteral("file")).toString());
        fragment.start = qMax(0.0, object.value(QStringLiteral("start")).toDouble(0.0));
        fragment.end = qMax(0.0, object.value(QStringLiteral("end")).toDouble(10.0));
        fragment.delayBefore = qMax(0.0, object.value(QStringLiteral("delayBefore")).toDouble(0.0));
        fragment.audioEnabled = object.value(QStringLiteral("audio")).toBool(true);
        fragment.volume = qBound(0.0, object.value(QStringLiteral("volume")).toDouble(1.0), 1.0);
        fragment.speed = qBound(0.1, object.value(QStringLiteral("speed")).toDouble(1.0), 4.0);
        fragment.label = object.value(QStringLiteral("label")).toString().trimmed();
        fragment.notes = object.value(QStringLiteral("notes")).toString().trimmed();
        fragment.delayColor = normalizedColor(object.value(QStringLiteral("delayColor")).toString());
        if (fragment.source.isValid()) {
            loaded.push_back(fragment);
        }
    }

    clearUndoRedoHistory();
    beginResetModel();
    m_items = std::move(loaded);
    endResetModel();
    const QString loadedName = root.value(QStringLiteral("name")).toString();
    const bool loadedRepeat = root.value(QStringLiteral("repeat")).toBool(false);
    const QDateTime now = currentTimestamp();
    QDateTime loadedCreatedAt = dateTimeFromJson(root.value(QStringLiteral("createdAt")));
    QDateTime loadedUpdatedAt = dateTimeFromJson(root.value(QStringLiteral("updatedAt")));
    if (!loadedCreatedAt.isValid()) {
        loadedCreatedAt = now;
    }
    if (!loadedUpdatedAt.isValid()) {
        loadedUpdatedAt = loadedCreatedAt;
    }
    const bool loadedNameChanged = m_name != loadedName;
    const bool loadedRepeatChanged = m_repeat != loadedRepeat;
    m_name = loadedName;
    m_repeat = loadedRepeat;
    setTimestamps(loadedCreatedAt, loadedUpdatedAt);
    const bool previousHasPlaylist = m_hasPlaylist;
    m_hasPlaylist = true;
    if (previousHasPlaylist != m_hasPlaylist) {
        emit hasPlaylistChanged();
    }
    if (loadedNameChanged) {
        emit nameChanged();
    }
    if (loadedRepeatChanged) {
        emit repeatChanged();
    }
    emit countChanged();
    emit totalDurationChanged();
    emit validityChanged();
    setFileUrl(fileUrl);
    setModified(false);
    return true;
}

bool PlaylistModel::save(const QUrl &fileUrl)
{
    if (!nameValid()) {
        emit saveFailed(nameValidationMessage());
        return false;
    }

    const QDateTime timestamp = currentTimestamp();
    const QDateTime createdAt = m_createdAt.isValid() ? m_createdAt : timestamp;
    setTimestamps(createdAt, timestamp);

    const QString path = localPathFromUrl(fileUrl);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit saveFailed(file.errorString());
        return false;
    }

    QJsonArray items;
    for (const Fragment &fragment : m_items) {
        QJsonObject object;
        object.insert(QStringLiteral("file"), fragment.source.toString());
        object.insert(QStringLiteral("start"), fragment.start);
        object.insert(QStringLiteral("end"), fragment.end);
        object.insert(QStringLiteral("delayBefore"), fragment.delayBefore);
        object.insert(QStringLiteral("audio"), fragment.audioEnabled);
        object.insert(QStringLiteral("volume"), fragment.volume);
        object.insert(QStringLiteral("speed"), fragment.speed);
        object.insert(QStringLiteral("label"), fragment.label);
        object.insert(QStringLiteral("notes"), fragment.notes);
        object.insert(QStringLiteral("delayColor"), fragment.delayColor);
        items.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("name"), m_name);
    root.insert(QStringLiteral("repeat"), m_repeat);
    root.insert(QStringLiteral("createdAt"), dateTimeToJson(m_createdAt));
    root.insert(QStringLiteral("updatedAt"), dateTimeToJson(m_updatedAt));
    root.insert(QStringLiteral("items"), items);

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    setFileUrl(fileUrl);
    setModified(false);
    return true;
}

QVariantMap PlaylistModel::get(int row) const
{
    QVariantMap values;
    if (row < 0 || row >= rowCount()) {
        return values;
    }

    const QModelIndex itemIndex = index(row);
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        values.insert(QString::fromUtf8(it.value()), data(itemIndex, it.key()));
    }

    return values;
}

QVariantList PlaylistModel::missingSources() const
{
    QVariantList missing;
    for (int row = 0; row < rowCount(); ++row) {
        const Fragment &fragment = m_items.at(static_cast<size_t>(row));
        if (!fragment.source.isLocalFile() || sourceExists(fragment)) {
            continue;
        }

        QVariantMap item;
        item.insert(QStringLiteral("row"), row);
        item.insert(QStringLiteral("source"), fragment.source);
        item.insert(QStringLiteral("fileName"), QFileInfo(fragment.source.toLocalFile()).fileName());
        item.insert(QStringLiteral("path"), fragment.source.toLocalFile());
        item.insert(QStringLiteral("label"), fragment.label);
        missing.append(item);
    }
    return missing;
}

void PlaylistModel::relinkSource(int row, const QUrl &source)
{
    if (row < 0 || row >= rowCount() || !source.isValid()) {
        return;
    }

    if (m_items[static_cast<size_t>(row)].source == source) {
        return;
    }

    pushUndoState();
    m_items[static_cast<size_t>(row)].source = source;

    const QModelIndex itemIndex = index(row);
    emit dataChanged(itemIndex, itemIndex, {
        SourceRole,
        FileNameRole,
        SourceStatusRole,
        ValidRole,
        ValidationMessageRole
    });
    emit validityChanged();
    setModified(true);
}

int PlaylistModel::relinkMissingFromFolder(const QUrl &folderUrl)
{
    const QString folderPath = localPathFromUrl(folderUrl);
    QFileInfo folderInfo(folderPath);
    if (!folderInfo.isDir()) {
        return 0;
    }

    QHash<QString, QString> candidates;
    QDirIterator iterator(folderPath, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QString fileName = QFileInfo(path).fileName();
        if (!candidates.contains(fileName)) {
            candidates.insert(fileName, path);
        }
    }

    int relinked = 0;
    bool historyRecorded = false;
    for (int row = 0; row < rowCount(); ++row) {
        Fragment &fragment = m_items[static_cast<size_t>(row)];
        if (!fragment.source.isLocalFile() || sourceExists(fragment)) {
            continue;
        }

        const QString fileName = QFileInfo(fragment.source.toLocalFile()).fileName();
        const QString replacement = candidates.value(fileName);
        if (replacement.isEmpty()) {
            continue;
        }

        if (!historyRecorded) {
            pushUndoState();
            historyRecorded = true;
        }
        fragment.source = QUrl::fromLocalFile(replacement);
        const QModelIndex itemIndex = index(row);
        emit dataChanged(itemIndex, itemIndex, {
            SourceRole,
            FileNameRole,
            SourceStatusRole,
            ValidRole,
            ValidationMessageRole
        });
        ++relinked;
    }

    if (relinked > 0) {
        emit validityChanged();
        setModified(true);
    }
    return relinked;
}

void PlaylistModel::updateFragment(int row, const QVariantMap &values)
{
    if (row < 0 || row >= rowCount()) {
        return;
    }

    const QModelIndex itemIndex = index(row);
    const QHash<int, QByteArray> names = roleNames();
    Fragment changed = m_items[static_cast<size_t>(row)];
    QVector<int> changedRoles;
    for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
        const QString key = QString::fromUtf8(it.value());
        if (values.contains(key)) {
            const Fragment beforeRole = changed;
            if (applyData(&changed, values.value(key), it.key())
                && (changed.source != beforeRole.source
                    || changed.start != beforeRole.start
                    || changed.end != beforeRole.end
                    || changed.delayBefore != beforeRole.delayBefore
                    || changed.audioEnabled != beforeRole.audioEnabled
                    || changed.volume != beforeRole.volume
                    || changed.speed != beforeRole.speed
                    || changed.label != beforeRole.label
                    || changed.notes != beforeRole.notes
                    || changed.delayColor != beforeRole.delayColor)) {
                changedRoles.append(it.key());
            }
        }
    }

    const Fragment &current = m_items.at(static_cast<size_t>(row));
    if (changedRoles.isEmpty()
        || (changed.source == current.source
            && changed.start == current.start
            && changed.end == current.end
            && changed.delayBefore == current.delayBefore
            && changed.audioEnabled == current.audioEnabled
            && changed.volume == current.volume
            && changed.speed == current.speed
            && changed.label == current.label
            && changed.notes == current.notes
            && changed.delayColor == current.delayColor)) {
        return;
    }

    pushUndoState();
    m_items[static_cast<size_t>(row)] = changed;
    changedRoles.append(DurationRole);
    changedRoles.append(SourceStatusRole);
    changedRoles.append(ValidRole);
    changedRoles.append(ValidationMessageRole);
    emit dataChanged(itemIndex, itemIndex, changedRoles);
    emit totalDurationChanged();
    emit validityChanged();
    setModified(true);
}

void PlaylistModel::undo()
{
    if (m_undoStack.isEmpty()) {
        return;
    }

    m_redoStack.append(captureState());
    const ModelState state = m_undoStack.takeLast();
    restoreState(state);
    emit undoRedoChanged();
}

void PlaylistModel::redo()
{
    if (m_redoStack.isEmpty()) {
        return;
    }

    m_undoStack.append(captureState());
    const ModelState state = m_redoStack.takeLast();
    restoreState(state);
    emit undoRedoChanged();
}

bool PlaylistModel::isValidName(const QString &name) const
{
    return isValidPlaylistName(normalizedPlaylistName(name));
}

bool PlaylistModel::applyData(Fragment *fragment, const QVariant &value, int role)
{
    if (!fragment) {
        return false;
    }

    switch (role) {
    case StartRole:
        fragment->start = qMax(0.0, value.toDouble());
        break;
    case EndRole:
        fragment->end = qMax(0.0, value.toDouble());
        break;
    case DelayBeforeRole:
        fragment->delayBefore = qMax(0.0, value.toDouble());
        break;
    case AudioEnabledRole:
        fragment->audioEnabled = value.toBool();
        break;
    case VolumeRole:
        fragment->volume = qBound(0.0, value.toDouble(), 1.0);
        break;
    case SpeedRole:
        fragment->speed = qBound(0.1, value.toDouble(), 4.0);
        break;
    case LabelRole:
        fragment->label = value.toString().trimmed();
        break;
    case NotesRole:
        fragment->notes = value.toString().trimmed();
        break;
    case DelayColorRole:
        fragment->delayColor = normalizedColor(value.toString());
        break;
    default:
        return false;
    }

    return true;
}

PlaylistModel::ModelState PlaylistModel::captureState() const
{
    return {
        m_items,
        m_name,
        m_repeat,
        m_createdAt,
        m_updatedAt,
        m_fileUrl
    };
}

void PlaylistModel::restoreState(const ModelState &state)
{
    const int previousCount = rowCount();
    const QString previousName = m_name;
    const bool previousRepeat = m_repeat;
    const QDateTime previousCreatedAt = m_createdAt;
    const QDateTime previousUpdatedAt = m_updatedAt;
    const QUrl previousFileUrl = m_fileUrl;

    beginResetModel();
    m_items = state.items;
    endResetModel();

    m_name = state.name;
    m_repeat = state.repeat;
    m_createdAt = state.createdAt;
    m_updatedAt = state.updatedAt;
    m_fileUrl = state.fileUrl;

    if (previousCount != rowCount()) {
        emit countChanged();
    emit totalDurationChanged();
    }
    if (previousName != m_name) {
        emit nameChanged();
    }
    if (previousRepeat != m_repeat) {
        emit repeatChanged();
    }
    if (previousCreatedAt != m_createdAt || previousUpdatedAt != m_updatedAt) {
        emit timestampsChanged();
    }
    if (previousFileUrl != m_fileUrl) {
        emit fileUrlChanged();
    }

    emit validityChanged();
    setModified(true);
}

void PlaylistModel::pushUndoState()
{
    m_undoStack.append(captureState());
    trimUndoHistory();
    if (!m_redoStack.isEmpty()) {
        m_redoStack.clear();
    }
    emit undoRedoChanged();
}

void PlaylistModel::clearUndoRedoHistory()
{
    if (m_undoStack.isEmpty() && m_redoStack.isEmpty()) {
        return;
    }

    m_undoStack.clear();
    m_redoStack.clear();
    emit undoRedoChanged();
}

void PlaylistModel::trimUndoHistory()
{
    constexpr int maxUndoSteps = 100;
    while (m_undoStack.size() > maxUndoSteps) {
        m_undoStack.removeFirst();
    }
}

bool PlaylistModel::isValidFragment(const Fragment &fragment)
{
    return sourceExists(fragment) && fragment.end > fragment.start;
}

QString PlaylistModel::validationMessage(const Fragment &fragment)
{
    if (!sourceExists(fragment)) {
        return QStringLiteral("Source file does not exist.");
    }

    if (fragment.end <= fragment.start) {
        return QStringLiteral("End time must be greater than start time.");
    }

    return {};
}

bool PlaylistModel::sourceExists(const Fragment &fragment)
{
    if (!fragment.source.isLocalFile()) {
        return true;
    }

    return QFileInfo::exists(fragment.source.toLocalFile());
}

QString PlaylistModel::sourceStatus(const Fragment &fragment)
{
    if (fragment.source.isLocalFile()) {
        return sourceExists(fragment) ? QStringLiteral("Online") : QStringLiteral("Offline");
    }

    return QStringLiteral("Unknown remote source");
}

void PlaylistModel::setModified(bool modified)
{
    if (m_modified == modified) {
        return;
    }

    m_modified = modified;
    emit modifiedChanged();
}

void PlaylistModel::setFileUrl(const QUrl &fileUrl)
{
    if (m_fileUrl == fileUrl) {
        return;
    }

    m_fileUrl = fileUrl;
    emit fileUrlChanged();
}

void PlaylistModel::setTimestamps(const QDateTime &createdAt, const QDateTime &updatedAt)
{
    const QDateTime normalizedCreatedAt = createdAt.toUTC();
    const QDateTime normalizedUpdatedAt = updatedAt.toUTC();
    if (m_createdAt == normalizedCreatedAt && m_updatedAt == normalizedUpdatedAt) {
        return;
    }

    m_createdAt = normalizedCreatedAt;
    m_updatedAt = normalizedUpdatedAt;
    emit timestampsChanged();
}

QString PlaylistModel::localPathFromUrl(const QUrl &fileUrl)
{
    return fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
}

QString PlaylistModel::normalizedPlaylistName(const QString &name)
{
    return name.trimmed();
}

bool PlaylistModel::isValidPlaylistName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_-]*$"));
    return pattern.match(name).hasMatch();
}

QDateTime PlaylistModel::currentTimestamp()
{
    return QDateTime::currentDateTimeUtc();
}

QDateTime PlaylistModel::dateTimeFromJson(const QJsonValue &value)
{
    if (!value.isString()) {
        return {};
    }

    QDateTime dateTime = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value.toString(), Qt::ISODate);
    }
    return dateTime.isValid() ? dateTime.toUTC() : QDateTime();
}

QString PlaylistModel::dateTimeToJson(const QDateTime &dateTime)
{
    return dateTime.toUTC().toString(Qt::ISODateWithMs);
}
