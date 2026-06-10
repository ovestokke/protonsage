#pragma once
#include <QString>
#include <QStringList>

namespace ProtonSage {

/// Returns common candidate Steam roots for a given home directory.
/// These are the well-known Linux Steam installation paths, including
/// XDG_DATA_HOME-aware native Steam plus Flatpak/Snap locations.
QStringList candidateRoots(const QString& home);

/// Returns explicit Steam roots from PROTONSAGE_STEAM_ROOTS.
/// Format: ':'-separated list. Non-existent paths are filtered by existingRoots().
QStringList envOverrideRoots();

/// Returns existing common Linux Steam roots for the current user.
/// Explicit PROTONSAGE_STEAM_ROOTS entries have highest priority.
/// Resolves symlinks and deduplicates so the same real path is not returned twice.
QStringList existingRoots();

/// Deduplicates a list of paths, resolving symlinks to detect aliases.
/// Empty strings are discarded.
QStringList dedupePaths(const QStringList& paths);

} // namespace ProtonSage
