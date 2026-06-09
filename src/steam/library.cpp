#include "library.h"
#include "paths.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace ProtonSage {

// ---------------------------------------------------------------------------
// Library folder discovery
// ---------------------------------------------------------------------------

QStringList libraryFoldersFromRoot(const QString& root)
{
    const QString cleanedRoot = QDir::cleanPath(root);
    const QString steamapps   = cleanedRoot + QStringLiteral("/steamapps");
    const QString vdfPath     = steamapps + QStringLiteral("/libraryfolders.vdf");

    VDFObject obj = parseVDFFile(vdfPath);
    if (obj.isEmpty()) {
        // If the VDF is absent but steamapps/ exists, use the root as sole library.
        QFileInfo info(steamapps);
        if (info.exists() && info.isDir()) {
            // Return the original root path (not the cleaned variant) for consistency,
            // but use QDir::cleanPath for the dedup check.
            return {cleanedRoot};
        }
        return {};
    }

    VDFObject foldersObj;
    if (!objectValueFold(obj, QStringLiteral("libraryfolders"), foldersObj))
        return {cleanedRoot};

    // Collect unique paths: primary root first, then secondary libraries.
    QStringList folders;
    folders.append(cleanedRoot);

    // Sort keys numerically by the index key ("0", "1", ...)
    QStringList keys = foldersObj.keys();
    std::sort(keys.begin(), keys.end(),
              [](const QString& a, const QString& b) {
                  bool okA = false, okB = false;
                  int ia = a.toInt(&okA);
                  int ib = b.toInt(&okB);
                  if (okA && okB) return ia < ib;
                  return a < b;
              });

    for (const QString& key : keys) {
        VDFObject entry;
        if (!asObject(foldersObj.value(key), entry))
            continue;

        QString path = stringValue(entry, QStringLiteral("path"));
        if (!path.isEmpty())
            folders.append(path);
    }

    return dedupePaths(folders);
}

// ---------------------------------------------------------------------------
// Main scan entry point
// ---------------------------------------------------------------------------

QList<Game> scanRoot(const QString& root)
{
    const QStringList libraries = libraryFoldersFromRoot(root);
    if (libraries.isEmpty())
        return {};

    // Read launch options from localconfig.vdf (best-effort, non-fatal)
    QMap<int, QString> launchOptions;
    try {
        launchOptions = launchOptionsFromRoot(root);
    } catch (...) {
        // ignore — existing launch options are optional display context
    }

    QList<Game> games;
    for (const QString& library : libraries) {
        const QList<Game> libraryGames = scanLibrary(library);
        for (Game g : libraryGames) {
            auto it = launchOptions.constFind(g.appId);
            if (it != launchOptions.constEnd())
                g.existingLaunchOptions = it.value();
            games.append(g);
        }
    }

    // Sort: case-insensitive name, then appId for ties
    std::sort(games.begin(), games.end(),
              [](const Game& a, const Game& b) {
                  int cmp = QString::compare(a.name, b.name, Qt::CaseInsensitive);
                  if (cmp != 0)
                      return cmp < 0;
                  return a.appId < b.appId;
              });
    return games;
}

// ---------------------------------------------------------------------------
// Library scanning
// ---------------------------------------------------------------------------

QList<Game> scanLibrary(const QString& libraryPath)
{
    const QString steamappsDir = QDir(libraryPath).filePath(QStringLiteral("steamapps"));
    QDir dir(steamappsDir);
    if (!dir.exists())
        return {};

    // Find all appmanifest_*.acf files
    QStringList filters;
    filters << QStringLiteral("appmanifest_*.acf");
    QStringList manifests = dir.entryList(filters, QDir::Files | QDir::Readable, QDir::Name);
    if (manifests.isEmpty())
        return {};

    const QString dirPath = dir.canonicalPath().isEmpty() ? dir.absolutePath() : dir.canonicalPath();

    QList<Game> games;
    games.reserve(manifests.size());

    for (const QString& manifest : manifests) {
        const QString fullPath = dirPath + QLatin1Char('/') + manifest;
        // Use the libraryPath as-given (not the canonical steamapps parent)
        Game g = parseAppManifestFile(fullPath, libraryPath);
        if (g.appId > 0)
            games.append(g);
    }

    return games;
}

// ---------------------------------------------------------------------------
// App manifest parsing
// ---------------------------------------------------------------------------

Game parseAppManifestFile(const QString& path, const QString& libraryPath)
{
    VDFObject obj = parseVDFFile(path);
    if (obj.isEmpty())
        return Game{};

    return parseAppManifest(obj, libraryPath);
}

Game parseAppManifest(const VDFObject& obj, const QString& libraryPath)
{
    VDFObject state;
    if (!objectValueFold(obj, QStringLiteral("AppState"), state))
        return Game{};

    Game game;
    game.libraryPath = QDir::cleanPath(libraryPath);
    game.launcher    = QStringLiteral("steam");

    // AppID
    bool ok = false;
    int appId = stringValue(state, QStringLiteral("appid")).toInt(&ok);
    if (!ok || appId <= 0)
        return Game{};
    game.appId = appId;

    game.name = stringValue(state, QStringLiteral("name"));

    // SizeOnDisk
    ok = false;
    qint64 sizeOnDisk = stringValue(state, QStringLiteral("SizeOnDisk")).toLongLong(&ok);
    if (ok)
        game.sizeOnDisk = sizeOnDisk;

    // StateFlags
    ok = false;
    qint64 stateFlags = stringValue(state, QStringLiteral("StateFlags")).toLongLong(&ok);
    if (ok)
        game.stateFlags = stateFlags;

    game.buildId = stringValue(state, QStringLiteral("buildid"));

    // InstallDir → InstallPath
    QString installDir = stringValue(state, QStringLiteral("installdir"));
    if (!installDir.isEmpty()) {
        game.installPath = QDir::cleanPath(game.libraryPath)
                           + QStringLiteral("/steamapps/common/")
                           + installDir;
    }

    return game;
}

// ---------------------------------------------------------------------------
// Launch options from localconfig.vdf
// ---------------------------------------------------------------------------

QMap<int, QString> launchOptionsFromRoot(const QString& root)
{
    QMap<int, QString> result;

    const QString pattern = QDir::cleanPath(root)
                            + QStringLiteral("/userdata/*/config/localconfig.vdf");

    QDirIterator it(QDir::cleanPath(root) + QStringLiteral("/userdata"),
                    QStringList() << QStringLiteral("localconfig.vdf"),
                    QDir::Files | QDir::Readable,
                    QDirIterator::Subdirectories);

    // Gather all matches sorted by path to ensure deterministic results
    QStringList paths;
    while (it.hasNext()) {
        QString p = it.next();
        if (p.endsWith(QStringLiteral("/config/localconfig.vdf")))
            paths.append(p);
    }
    paths.sort();

    for (const QString& path : paths) {
        QMap<int, QString> options = parseLocalConfigLaunchOptionsFile(path);
        for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
            // First-write-first-kept: the first user's value sticks
            if (!result.contains(it.key()))
                result.insert(it.key(), it.value());
        }
    }

    return result;
}

QMap<int, QString> parseLocalConfigLaunchOptionsFile(const QString& path)
{
    VDFObject obj = parseVDFFile(path);
    if (obj.isEmpty())
        return {};

    return parseLocalConfigLaunchOptions(obj);
}

// Internal helpers for localconfig parsing
namespace {

/// Navigates into UserLocalConfigStore → Software → Valve → Steam → apps.
bool localConfigAppsObject(const VDFObject& obj, VDFObject& out)
{
    VDFObject current;
    if (!objectValueFold(obj, QStringLiteral("UserLocalConfigStore"), current))
        return false;

    QStringList path = {
        QStringLiteral("Software"),
        QStringLiteral("Valve"),
        QStringLiteral("Steam"),
        QStringLiteral("apps")
    };

    for (const QString& key : path) {
        VDFObject next;
        if (!objectValueFold(current, key, next))
            return false;
        current = next;
    }

    out = current;
    return true;
}

/// Collects LaunchOptions from each app entry under the apps object.
void collectLaunchOptionsFromApps(const VDFObject& apps, QMap<int, QString>& result)
{
    QStringList keys = apps.keys();
    std::sort(keys.begin(), keys.end());

    for (const QString& key : keys) {
        bool ok = false;
        int appId = key.toInt(&ok);
        if (!ok || appId <= 0)
            continue;

        VDFObject entry;
        if (!asObject(apps.value(key), entry))
            continue;

        QString value = stringValueFold(entry, {
            QStringLiteral("LaunchOptions"),
            QStringLiteral("launchoptions"),
            QStringLiteral("launchOptions"),
            QStringLiteral("Launch Options")
        });

        if (!value.isEmpty())
            result.insert(appId, value);
    }
}

/// Recursive fallback for small fixture variants / future Steam layout changes.
void collectLaunchOptionsRecursive(const VDFObject& obj, QMap<int, QString>& result)
{
    QStringList keys = obj.keys();
    std::sort(keys.begin(), keys.end());

    for (const QString& key : keys) {
        VDFObject child;
        if (!asObject(obj.value(key), child))
            continue;

        // If the key is numeric it's likely an app id — try to extract LaunchOptions
        bool ok = false;
        int appId = key.toInt(&ok);
        if (ok && appId > 0 && !result.contains(appId)) {
            QString value = stringValueFold(child, {
                QStringLiteral("LaunchOptions"),
                QStringLiteral("launchoptions"),
                QStringLiteral("launchOptions"),
                QStringLiteral("Launch Options")
            });
            if (!value.isEmpty())
                result.insert(appId, value);
        }

        // Recurse deeper
        collectLaunchOptionsRecursive(child, result);
    }
}

} // anonymous namespace

QMap<int, QString> parseLocalConfigLaunchOptions(const VDFObject& obj)
{
    QMap<int, QString> result;

    // Primary path: UserLocalConfigStore → Software → Valve → Steam → apps
    VDFObject apps;
    if (localConfigAppsObject(obj, apps))
        collectLaunchOptionsFromApps(apps, result);

    // Fallback: recursive scan for any numeric key with LaunchOptions
    collectLaunchOptionsRecursive(obj, result);

    return result;
}

} // namespace ProtonSage
