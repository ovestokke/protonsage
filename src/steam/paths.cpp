#include "paths.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ProtonSage {

static QString realPathKey(const QString& path)
{
    QFileInfo info(QDir::cleanPath(path));
    if (info.exists()) {
        QString real = info.canonicalFilePath();
        if (!real.isEmpty())
            return QDir::cleanPath(real);
    }
    return QDir::cleanPath(path);
}

QStringList candidateRoots(const QString& home)
{
    if (home.isEmpty())
        return {};

    QString dataHome = QString::fromUtf8(qgetenv("XDG_DATA_HOME")).trimmed();
    if (dataHome.isEmpty())
        dataHome = home + QStringLiteral("/.local/share");

    QStringList roots;
    roots << home + QStringLiteral("/.steam/steam")
          << home + QStringLiteral("/.steam/root")
          << QDir(dataHome).filePath(QStringLiteral("Steam"))
          << home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam")
          << home + QStringLiteral("/snap/steam/common/.local/share/Steam");
    return roots;
}

QStringList envOverrideRoots()
{
    QString raw = QString::fromUtf8(qgetenv("PROTONSAGE_STEAM_ROOTS"));
    if (raw.trimmed().isEmpty())
        return {};

    QStringList roots;
    for (const QString& part : raw.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        QString path = QDir::cleanPath(part.trimmed());
        if (!path.isEmpty())
            roots.append(path);
    }
    return roots;
}

QStringList existingRoots()
{
    const QString home = QDir::homePath();
    if (home.isEmpty())
        return {};

    QSet<QString> seenReal;
    QStringList roots;

    auto addIfExisting = [&](const QString& candidate) {
        QFileInfo info(candidate);
        if (!info.exists() || !info.isDir())
            return;

        QString key = realPathKey(candidate);
        if (seenReal.contains(key))
            return;

        seenReal.insert(key);
        roots.append(QDir::cleanPath(candidate));
    };

    for (const QString& candidate : envOverrideRoots())
        addIfExisting(candidate);
    for (const QString& candidate : candidateRoots(home))
        addIfExisting(candidate);

    return roots;
}

QStringList dedupePaths(const QStringList& paths)
{
    QSet<QString> seen;
    QStringList out;
    for (const QString& path : paths) {
        if (path.isEmpty())
            continue;

        const QString cleaned = QDir::cleanPath(path);
        QString key = cleaned;

        // Try to resolve symlinks for deduplication key
        QFileInfo info(cleaned);
        if (info.exists()) {
            QString realPath = info.canonicalFilePath();
            if (!realPath.isEmpty())
                key = QDir::cleanPath(realPath);
        }

        if (seen.contains(key))
            continue;

        seen.insert(key);
        out.append(cleaned);
    }
    return out;
}

} // namespace ProtonSage
