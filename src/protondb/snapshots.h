#pragma once
#include <QString>
#include <QDateTime>
#include <QDate>
#include <QList>
#include <QNetworkAccessManager>

namespace ProtonSage {

// ── Snapshot metadata ─────────────────────────────────────────────────

struct Snapshot {
    QString filename;
    QDate date;
    QString url;
    qint64 size = 0;
};

// ── API URLs ──────────────────────────────────────────────────────────

/// GitHub contents API endpoint for the reports directory.
inline const QString kReportsContentsAPI =
    QStringLiteral("https://api.github.com/repos/bdefore/protondb-data/contents/reports");

/// Raw.githubusercontent.com base URL for downloading report archives.
inline const QString kReportsRawBaseURL =
    QStringLiteral("https://raw.githubusercontent.com/bdefore/protondb-data/master/reports");

// ── Functions ─────────────────────────────────────────────────────────

/**
 * Parse a snapshot filename like reports_jun1_2026.tar.gz and extract
 * the date.  Returns an invalid QDate on failure.
 */
QDate parseSnapshotFilename(const QString& filename);

/**
 * Given a list of filenames, return the newest valid snapshot or a
 * default-constructed Snapshot (date.isNull() == true) if none matched.
 */
Snapshot selectLatestSnapshot(const QStringList& filenames);

/**
 * Sort a list of Snapshots in-place newest-first (by date descending;
 * ties broken by filename descending).
 */
void sortSnapshotsNewestFirst(QList<Snapshot>& snapshots);

/**
 * Query the GitHub contents API to list ProtonDB report archives.
 * Only reads directory metadata; does not download archives.
 *
 * Returns the list sorted newest-first, or an empty list on error
 * (logged via qWarning).
 */
QList<Snapshot> listSnapshotsFromGitHub(QNetworkAccessManager& nam);

/**
 * Convenience: return the single newest snapshot from GitHub.
 * If no snapshots are found the returned Snapshot has an invalid date.
 */
Snapshot latestSnapshotFromGitHub(QNetworkAccessManager& nam);

} // namespace ProtonSage
