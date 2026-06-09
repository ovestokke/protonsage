#include "paths.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ProtonSage {

QStringList candidateRoots(const QString& home)
{
    if (home.isEmpty())
        return {};

    QStringList roots;
    roots << home + QStringLiteral("/.steam/steam")
          << home + QStringLiteral("/.steam/root")
          << home + QStringLiteral("/.local/share/Steam")
          << home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam");
    return roots;
}

QStringList existingRoots()
{
    const QString home = QDir::homePath();
    if (home.isEmpty())
        return {};

    QSet<QString> seenReal;
    QStringList roots;

    for (const QString& candidate : candidateRoots(home)) {
        QFileInfo info(candidate);
        if (!info.exists() || !info.isDir())
            continue;

        // Resolve symlinks to deduplicate (~/.steam/steam vs ~/.steam/root etc.)
        QString realPath = info.canonicalFilePath(); // resolves symlinks
        if (realPath.isEmpty())
            realPath = QDir::cleanPath(candidate);

        if (seenReal.contains(realPath))
            continue;

        seenReal.insert(realPath);
        roots.append(realPath);
    }

    return dedupePaths(roots);
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
