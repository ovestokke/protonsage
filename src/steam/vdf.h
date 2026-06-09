#pragma once
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QByteArray>

namespace ProtonSage {

/// Typedef for Valve KeyValues (VDF) object.
/// Values are either QString (leaf) or nested VDFObject (block).
using VDFObject = QVariantMap;

// ── VDF Parsing ──────────────────────────────────────────────────────

/// Parse VDF data from a byte array.
/// Supports the subset of Valve KeyValues used by Steam libraryfolders.vdf,
/// appmanifest_*.acf, and localconfig.vdf.
VDFObject parseVDF(const QByteArray& data);

/// Parse a VDF/ACF file from disk (read-only).
VDFObject parseVDFFile(const QString& path);

// ── VDF Access Helpers ──────────────────────────────────────────────

/// Try to cast a VDF value to a nested object.
/// Returns true and sets @p out if the value represents an object.
bool asObject(const QVariant& value, VDFObject& out);

/// Returns the string value for a key, or empty string if missing or not a string.
QString stringValue(const VDFObject& obj, const QString& key);

/// Case-insensitive key lookup returning string value.
/// Tries exact match first, then case-insensitive scan.
QString stringValueFold(const VDFObject& obj, const QStringList& keys);

/// Case-insensitive key lookup returning nested object.
/// Tries exact match first, then case-insensitive scan.
bool objectValueFold(const VDFObject& obj, const QString& key, VDFObject& out);

} // namespace ProtonSage
