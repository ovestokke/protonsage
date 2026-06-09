#pragma once
#include <QString>
#include <QStringList>

namespace ProtonSage {

/// Returns common candidate Steam roots for a given home directory.
/// These are the well-known Linux Steam installation and Flatpak paths.
QStringList candidateRoots(const QString& home);

/// Returns existing common Linux Steam roots for the current user.
/// Resolves symlinks and deduplicates so the same real path is not returned twice.
QStringList existingRoots();

/// Deduplicates a list of paths, resolving symlinks to detect aliases.
/// Empty strings are discarded.
QStringList dedupePaths(const QStringList& paths);

} // namespace ProtonSage
