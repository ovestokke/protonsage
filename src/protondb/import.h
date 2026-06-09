#pragma once
#include <QString>
#include <QDateTime>
#include <QDate>
#include <QIODevice>
#include "storage/database.h"

namespace ProtonSage {

// ── Import metadata ───────────────────────────────────────────────────

struct SnapshotImportMeta {
    QString sourceId;
    QString snapshotFilename;
    QDate snapshotDate;
    QString sourceUrl;
    QString licenseNote;
};

// ── Import result summary ─────────────────────────────────────────────

struct ImportResult {
    qint64 importRunId = 0;
    QString sourceId;
    QString snapshotFilename;
    int gamesImported = 0;
    int reportsImported = 0;
    int systemInfoImported = 0;
    int recordsSkipped = 0;
};

// ── Functions ─────────────────────────────────────────────────────────

/// Default ODbL/DbCL license notice for ProtonDB data.
const QString& dataLicenseNote();

/**
 * Import a ProtonDB reports_*.tar.gz snapshot into the database.
 *
 * The stream must be a readable gzip-compressed tar archive containing
 * reports_piiremoved.json.  All database writes happen inside a single
 * SQLite transaction.
 *
 * @param db     Target database.
 * @param stream Gzip-compressed tar archive data.
 * @param meta   Import metadata (filename, date, source, etc.).
 * @param error  If non-null, receives a human-readable error message
 *               on failure.
 * @return       ImportResult with counts.  On error the returned
 *               result is zero-valued and *error describes the problem.
 */
ImportResult importSnapshot(Database& db, QIODevice& stream,
                            const SnapshotImportMeta& meta,
                            QString* error = nullptr);

} // namespace ProtonSage
