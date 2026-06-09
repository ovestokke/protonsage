#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include "core/models.h"

namespace ProtonSage {

/// Performs read-only best-effort local system detection.
/// Populates GPU, CPU, RAM, distro, kernel, session type, and normalized
/// comparison categories on the returned SystemProfile.
SystemProfile detectProfile();

/// Parses /etc/os-release style key=value data.
QMap<QString, QString> parseOSRelease(const QString& data);

/// Extracts the first CPU model string from /proc/cpuinfo data.
QString parseCPUInfo(const QString& data);

/// Returns MemTotal from /proc/meminfo as GiB rounded to one decimal place.
double parseMemInfoGB(const QString& data);

/// Extracts GPU vendor and model from lspci output.
/// @param vendor  set to "NVIDIA", "AMD", "Intel", or empty
/// @param model   set to the human-readable GPU description
void parseLspciGPU(const QString& data, QString& vendor, QString& model);

// ── Normalization helpers (exposed for reuse by advisor/system comparison) ──

/// Coarse GPU vendor classification from a combined vendor+model string.
QString normalizeGPUVendor(const QString& gpuVendorAndModel);

/// Simplifies a GPU model string by stripping brackets, parens, and vendor prefixes.
QString simplifyGPUModel(const QString& model);

/// Coarse CPU vendor from CPU model string.
QString normalizeCPUVendor(const QString& cpu);

/// Coarse CPU class (e.g. "ryzen 7", "core i7", "unknown") from model string.
QString normalizeCPUClass(const QString& cpu);

/// RAM size bucket: "unknown", "<8", "8-15", "16-31", "32+"
QString ramBucket(double ramGB);

/// Normalizes distro string to a family: "arch", "fedora", "ubuntu", "debian", "unknown"
QString normalizeDistroFamily(const QStringList& values);

/// Normalizes session type: "wayland", "x11", "unknown"
QString normalizeSessionType(const QString& value);

/// Extracts major.minor version string (e.g. "6.8" from "6.8.0-arch1-1").
QString versionMajorMinor(const QString& value);

/// Parses a RAM string that may include unit suffix (GB, MB, KB) and returns GiB.
double parseRAMGB(const QString& value);

/// Populates the normalized comparison fields on a SystemProfile.
void normalizeSystemProfile(SystemProfile& profile);

} // namespace ProtonSage
