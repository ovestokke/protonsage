#include "profile.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>
#include <cstdio>
#include <cstdlib>

namespace ProtonSage {

// ---------------------------------------------------------------------------
// /sys/class/drm GPU detection helper
// ---------------------------------------------------------------------------

/// Attempts to discover GPU vendor and model via /sys/class/drm.
/// Returns true if at least vendor was detected.
static bool detectGPUFromSysfs(QString& vendor, QString& model)
{
    QDir drmDir(QStringLiteral("/sys/class/drm"));
    if (!drmDir.exists())
        return false;

    const QStringList entries = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        // Skip render nodes and other non-card entries
        if (!entry.startsWith(QStringLiteral("card")))
            continue;

        QString devicePath = drmDir.absoluteFilePath(entry) + QStringLiteral("/device");
        QFileInfo devInfo(devicePath);
        if (!devInfo.exists() || !devInfo.isSymLink())
            continue;

        // Read vendor ID
        QFile vendorFile(devicePath + QStringLiteral("/vendor"));
        if (!vendorFile.open(QIODevice::ReadOnly))
            continue;

        QString vendorId = QString::fromUtf8(vendorFile.readAll()).trimmed();
        vendorFile.close();

        if (vendorId == QStringLiteral("0x1002")) {
            vendor = QStringLiteral("AMD");
        } else if (vendorId == QStringLiteral("0x10de")) {
            vendor = QStringLiteral("NVIDIA");
        } else if (vendorId == QStringLiteral("0x8086")) {
            vendor = QStringLiteral("Intel");
        } else {
            continue; // unknown vendor, try next card
        }

        // Try to read the device name from the driver symlink or uevent
        QFile ueventFile(devicePath + QStringLiteral("/uevent"));
        if (ueventFile.open(QIODevice::ReadOnly)) {
            QTextStream in(&ueventFile);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith(QStringLiteral("DRIVER="))) {
                    QString driver = line.mid(7).trimmed();
                    if (!driver.isEmpty() && model.isEmpty())
                        model = driver;
                }
            }
            ueventFile.close();
        }

        // Try reading a more descriptive name from device subdirectories
        // (amdgpu, nvidia, i915 driver-specific paths)
        const QString driverPath = devicePath + QStringLiteral("/driver");
        QFileInfo driverInfo(driverPath);
        if (driverInfo.exists() && driverInfo.isSymLink()) {
            QString driverName = driverInfo.symLinkTarget();
            int lastSlash = driverName.lastIndexOf(QLatin1Char('/'));
            if (lastSlash >= 0)
                driverName = driverName.mid(lastSlash + 1);
            if (model.isEmpty())
                model = driverName;
        }

        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// lspci parsing
// ---------------------------------------------------------------------------

void parseLspciGPU(const QString& data, QString& vendor, QString& model)
{
    QTextStream in(const_cast<QString*>(&data), QIODevice::ReadOnly);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        QString lower = line.toLower();

        if (!lower.contains(QStringLiteral("vga compatible controller"))
            && !lower.contains(QStringLiteral("3d controller"))
            && !lower.contains(QStringLiteral("display controller"))) {
            continue;
        }

        model = line;
        int idx = line.indexOf(QStringLiteral(": "));
        if (idx >= 0 && idx + 2 < line.size())
            model = line.mid(idx + 2);

        if (lower.contains(QStringLiteral("nvidia")))
            vendor = QStringLiteral("NVIDIA");
        else if (lower.contains(QStringLiteral("advanced micro devices"))
                 || lower.contains(QStringLiteral("amd/ati"))
                 || lower.contains(QStringLiteral("radeon")))
            vendor = QStringLiteral("AMD");
        else if (lower.contains(QStringLiteral("intel")))
            vendor = QStringLiteral("Intel");

        return;
    }
}

// ---------------------------------------------------------------------------
// OS release parsing
// ---------------------------------------------------------------------------

QMap<QString, QString> parseOSRelease(const QString& data)
{
    QMap<QString, QString> fields;
    QTextStream in(const_cast<QString*>(&data), QIODevice::ReadOnly);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        int eqPos = line.indexOf(QLatin1Char('='));
        if (eqPos < 0)
            continue;

        QString key = line.left(eqPos).trimmed();
        QString value = line.mid(eqPos + 1).trimmed();
        // Unquote
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            value = value.mid(1, value.size() - 2);
        value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        fields.insert(key, value);
    }
    return fields;
}

static QString distroName(const QMap<QString, QString>& fields)
{
    auto it = fields.constFind(QStringLiteral("PRETTY_NAME"));
    if (it != fields.constEnd() && !it->isEmpty())
        return it.value();

    it = fields.constFind(QStringLiteral("NAME"));
    if (it != fields.constEnd() && !it->isEmpty()) {
        QString name = it.value();
        auto vit = fields.constFind(QStringLiteral("VERSION_ID"));
        if (vit != fields.constEnd() && !vit->isEmpty())
            return name + QStringLiteral(" ") + vit.value();
        return name;
    }

    return QString();
}

// ---------------------------------------------------------------------------
// CPU info parsing
// ---------------------------------------------------------------------------

QString parseCPUInfo(const QString& data)
{
    QTextStream in(const_cast<QString*>(&data), QIODevice::ReadOnly);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith(QStringLiteral("model name")) || line.startsWith(QStringLiteral("Hardware"))) {
            int colon = line.indexOf(QLatin1Char(':'));
            if (colon >= 0 && colon + 1 < line.size())
                return line.mid(colon + 1).trimmed();
        }
    }
    return QString();
}

// ---------------------------------------------------------------------------
// Memory parsing
// ---------------------------------------------------------------------------

double parseMemInfoGB(const QString& data)
{
    QTextStream in(const_cast<QString*>(&data), QIODevice::ReadOnly);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.startsWith(QStringLiteral("MemTotal:")))
            continue;

        QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 2)
            return 0;

        bool ok = false;
        double kb = parts[1].toDouble(&ok);
        if (!ok)
            return 0;

        double gb = kb / 1024.0 / 1024.0;
        // Round to 1 decimal
        return qRound(gb * 10.0) / 10.0;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Command execution
// ---------------------------------------------------------------------------

static QString runOutput(int timeoutMs, const QString& program, const QStringList& args = {})
{
    // Check if the executable exists
    if (program.contains(QLatin1Char('/'))) {
        if (!QFileInfo::exists(program))
            return QString();
    } else {
        // Look up in PATH
        QProcess which;
        which.start(QStringLiteral("which"), QStringList{program});
        which.waitForFinished(3000);
        if (which.exitCode() != 0)
            return QString();
    }

    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(timeoutMs))
        return QString();

    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return out;
}

// ---------------------------------------------------------------------------
// Normalization helpers
// ---------------------------------------------------------------------------

QString normalizeGPUVendor(const QString& gpuVendorAndModel)
{
    QString lower = gpuVendorAndModel.toLower();
    if (lower.contains(QStringLiteral("nvidia"))
        || lower.contains(QStringLiteral("geforce"))
        || lower.contains(QStringLiteral("rtx"))
        || lower.contains(QStringLiteral("gtx")))
        return QStringLiteral("nvidia");

    if (lower.contains(QStringLiteral("advanced micro devices"))
        || lower.contains(QStringLiteral("amd/ati"))
        || lower.contains(QStringLiteral("amd"))
        || lower.contains(QStringLiteral("radeon"))
        || lower.contains(QStringLiteral("radv")))
        return QStringLiteral("amd");

    if (lower.contains(QStringLiteral("intel"))
        || lower.contains(QStringLiteral("arc"))
        || lower.contains(QStringLiteral("iris"))
        || lower.contains(QStringLiteral("uhd graphics")))
        return QStringLiteral("intel");

    return QStringLiteral("unknown");
}

QString simplifyGPUModel(const QString& model)
{
    QString value = model.trimmed();
    if (value.isEmpty())
        return value;

    // Extract content inside brackets []
    {
        int start = value.indexOf(QLatin1Char('['));
        if (start >= 0) {
            int end = value.indexOf(QLatin1Char(']'), start + 1);
            if (end >= 0)
                value = value.mid(start + 1, end - start - 1);
        }
    }

    // Strip trailing parenthetical
    {
        int idx = value.indexOf(QLatin1Char('('));
        if (idx >= 0)
            value = value.left(idx).trimmed();
    }

    QString lower = value.toLower();
    QStringList replacers = {
        QStringLiteral("nvidia corporation"),
        QStringLiteral("nvidia"),
        QStringLiteral("advanced micro devices, inc."),
        QStringLiteral("advanced micro devices"),
        QStringLiteral("amd/ati"),
        QStringLiteral("amd"),
        QStringLiteral("ati technologies inc."),
        QStringLiteral("intel corporation"),
        QStringLiteral("intel"),
    };
    for (const QString& old : replacers)
        lower.replace(old, QStringLiteral(" "));

    lower.replace(QLatin1Char('['), QLatin1Char(' '));
    lower.replace(QLatin1Char(']'), QLatin1Char(' '));
    lower.replace(QLatin1Char(','), QLatin1Char(' '));

    // Collapse whitespace
    QStringList words = lower.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    return words.join(QLatin1Char(' '));
}

QString normalizeCPUVendor(const QString& cpu)
{
    QString lower = cpu.toLower();
    if (lower.contains(QStringLiteral("amd"))
        || lower.contains(QStringLiteral("ryzen"))
        || lower.contains(QStringLiteral("epyc")))
        return QStringLiteral("amd");

    if (lower.contains(QStringLiteral("intel"))
        || lower.contains(QStringLiteral("core(tm)"))
        || lower.contains(QStringLiteral("core i"))
        || lower.contains(QStringLiteral("xeon")))
        return QStringLiteral("intel");

    if (lower.contains(QStringLiteral("apple")))
        return QStringLiteral("apple");

    return QStringLiteral("unknown");
}

QString normalizeCPUClass(const QString& cpu)
{
    QString lower = cpu.toLower();

    if (lower.contains(QStringLiteral("ryzen 9")))
        return QStringLiteral("ryzen 9");
    if (lower.contains(QStringLiteral("ryzen 7")))
        return QStringLiteral("ryzen 7");
    if (lower.contains(QStringLiteral("ryzen 5")))
        return QStringLiteral("ryzen 5");
    if (lower.contains(QStringLiteral("ryzen 3")))
        return QStringLiteral("ryzen 3");
    if (lower.contains(QStringLiteral("threadripper")))
        return QStringLiteral("threadripper");
    if (lower.contains(QStringLiteral("epyc")))
        return QStringLiteral("epyc");
    if (lower.contains(QStringLiteral("core ultra")))
        return QStringLiteral("core ultra");
    if (lower.contains(QStringLiteral("core i9")) || lower.contains(QStringLiteral("i9-")))
        return QStringLiteral("core i9");
    if (lower.contains(QStringLiteral("core i7")) || lower.contains(QStringLiteral("i7-")))
        return QStringLiteral("core i7");
    if (lower.contains(QStringLiteral("core i5")) || lower.contains(QStringLiteral("i5-")))
        return QStringLiteral("core i5");
    if (lower.contains(QStringLiteral("core i3")) || lower.contains(QStringLiteral("i3-")))
        return QStringLiteral("core i3");
    if (lower.contains(QStringLiteral("xeon")))
        return QStringLiteral("xeon");

    return QStringLiteral("unknown");
}

QString ramBucket(double ramGB)
{
    if (ramGB <= 0)
        return QStringLiteral("unknown");
    if (ramGB < 8)
        return QStringLiteral("<8");
    if (ramGB < 16)
        return QStringLiteral("8-15");
    if (ramGB < 32)
        return QStringLiteral("16-31");
    return QStringLiteral("32+");
}

QString normalizeDistroFamily(const QStringList& values)
{
    QString combined = values.join(QLatin1Char(' ')).toLower();

    if (combined.contains(QStringLiteral("arch"))
        || combined.contains(QStringLiteral("manjaro"))
        || combined.contains(QStringLiteral("endeavour"))
        || combined.contains(QStringLiteral("steamos")))
        return QStringLiteral("arch");

    if (combined.contains(QStringLiteral("fedora"))
        || combined.contains(QStringLiteral("nobara"))
        || combined.contains(QStringLiteral("bazzite"))
        || combined.contains(QStringLiteral("ublue")))
        return QStringLiteral("fedora");

    if (combined.contains(QStringLiteral("ubuntu"))
        || combined.contains(QStringLiteral("pop!_os"))
        || combined.contains(QStringLiteral("pop os"))
        || combined.contains(QStringLiteral("mint"))
        || combined.contains(QStringLiteral("neon")))
        return QStringLiteral("ubuntu");

    if (combined.contains(QStringLiteral("debian")))
        return QStringLiteral("debian");

    return QStringLiteral("unknown");
}

QString normalizeSessionType(const QString& value)
{
    QString lower = value.toLower();
    if (lower.contains(QStringLiteral("wayland")))
        return QStringLiteral("wayland");
    if (lower.contains(QStringLiteral("x11")) || lower.contains(QStringLiteral("xorg")))
        return QStringLiteral("x11");
    return QStringLiteral("unknown");
}

QString versionMajorMinor(const QString& value)
{
    static const QRegularExpression re(QStringLiteral("(\\d+)\\.(\\d+)"),
                                       QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(value);
    if (!m.hasMatch())
        return QString();
    return m.captured(1) + QStringLiteral(".") + m.captured(2);
}

double parseRAMGB(const QString& value)
{
    QString s = value.trimmed().toLower();
    if (s.isEmpty())
        return 0;

    // Detect unit
    QString unit = QStringLiteral("gb");
    if (s.contains(QStringLiteral("mib")) || s.contains(QStringLiteral("mb")))
        unit = QStringLiteral("mb");
    else if (s.contains(QStringLiteral("kib")) || s.contains(QStringLiteral("kb")))
        unit = QStringLiteral("kb");

    // Extract first number
    static const QRegularExpression numRe(QStringLiteral("[-+]?\\d+(?:\\.\\d+)?"));
    QRegularExpressionMatch m = numRe.match(s);
    if (!m.hasMatch())
        return 0;

    bool ok = false;
    double parsed = m.captured(0).toDouble(&ok);
    if (!ok)
        return 0;

    if (unit == QStringLiteral("mb"))
        return parsed / 1024.0;
    if (unit == QStringLiteral("kb"))
        return parsed / 1024.0 / 1024.0;
    return parsed;
}

// ---------------------------------------------------------------------------
// Full profile normalization
// ---------------------------------------------------------------------------

void normalizeSystemProfile(SystemProfile& profile)
{
    // Build distro values for family detection
    QStringList distroValues;
    {
        auto it = profile.raw.constFind(QStringLiteral("os-release.ID"));
        if (it != profile.raw.constEnd()) distroValues.append(it.value());
        it = profile.raw.constFind(QStringLiteral("os-release.ID_LIKE"));
        if (it != profile.raw.constEnd()) distroValues.append(it.value());
        if (!profile.distro.isEmpty()) distroValues.append(profile.distro);
    }

    profile.normGpuVendor = normalizeGPUVendor(profile.gpuVendor + QStringLiteral(" ") + profile.gpuModel);
    profile.normGpuModel  = simplifyGPUModel(profile.gpuModel);
    profile.normGpuDriver = versionMajorMinor(profile.gpuDriver);
    profile.normCpuVendor = normalizeCPUVendor(profile.cpu);
    profile.normCpuClass  = normalizeCPUClass(profile.cpu);
    profile.normRamBucket = ramBucket(profile.ramGb);
    profile.normDistroFamily = normalizeDistroFamily(distroValues);
    profile.normKernel    = versionMajorMinor(profile.kernel);

    // Session type: prefer XDG_SESSION_TYPE, fall back to desktop env
    QString session = profile.sessionType;
    if (session.isEmpty())
        session = profile.desktop;
    profile.normSessionType = normalizeSessionType(session);
}

// ---------------------------------------------------------------------------
// Main detection entry point
// ---------------------------------------------------------------------------

SystemProfile detectProfile()
{
    SystemProfile profile;

    // ── Distro ──────────────────────────────────────────────────────
    {
        QFile file(QStringLiteral("/etc/os-release"));
        if (file.open(QIODevice::ReadOnly)) {
            QString data = QString::fromUtf8(file.readAll());
            file.close();
            QMap<QString, QString> fields = parseOSRelease(data);
            profile.distro = distroName(fields);
            for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
                profile.raw.insert(QStringLiteral("os-release.") + it.key(), it.value());
        }
    }

    // ── Kernel ──────────────────────────────────────────────────────
    {
        QFile f("/proc/version");
        if (f.open(QIODevice::ReadOnly)) {
            QString line = QString::fromUtf8(f.readAll()).trimmed();
            // "Linux version 7.0.10-2-cachyos ..."
            if (line.startsWith("Linux version "))
                profile.kernel = line.mid(14).section(' ', 0, 0);
        }
    }

    // ── CPU ─────────────────────────────────────────────────────────
    {
        QFile file(QStringLiteral("/proc/cpuinfo"));
        if (file.open(QIODevice::ReadOnly)) {
            QString data = QString::fromUtf8(file.readAll());
            file.close();
            profile.cpu = parseCPUInfo(data);
        }
    }

    // ── RAM ─────────────────────────────────────────────────────────
    {
        QFile file(QStringLiteral("/proc/meminfo"));
        if (file.open(QIODevice::ReadOnly)) {
            QString data = QString::fromUtf8(file.readAll());
            file.close();
            profile.ramGb = parseMemInfoGB(data);
        }
    }

    // ── GPU ─────────────────────────────────────────────────────────
    {
        // Read-only from /sys without directory traversal
        QString gpuVendor, gpuModel, gpuDriver;
        
        // Try /sys/class/drm/cardN/device/vendor
        for (int n = 0; n <= 9; ++n) {
            QFile vf(QString("/sys/class/drm/card%1/device/vendor").arg(n));
            if (vf.open(QIODevice::ReadOnly)) {
                QString vid = QString::fromUtf8(vf.readAll()).trimmed();
                if (vid == "0x1002") gpuVendor = "AMD";
                else if (vid == "0x10de") gpuVendor = "NVIDIA";
                else if (vid == "0x8086") gpuVendor = "Intel";
                if (!gpuVendor.isEmpty()) break;
            }
        }
        profile.gpuVendor = gpuVendor;
        profile.gpuModel  = gpuModel;

        // GPU model name — vendor-specific paths
        if (!gpuVendor.isEmpty() && gpuModel.isEmpty()) {
            if (gpuVendor == "NVIDIA") {
                QDir gpuDir("/proc/driver/nvidia/gpus");
                for (const QString& entry : gpuDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    QFile infoFile(gpuDir.absoluteFilePath(entry) + "/information");
                    if (infoFile.open(QIODevice::ReadOnly)) {
                        QString data = QString::fromUtf8(infoFile.readAll());
                        for (const QString& line : data.split('\n')) {
                            if (line.startsWith("Model:")) {
                                QString model = line.mid(6).trimmed();
                                int geforce = model.indexOf("GeForce ");
                                if (geforce >= 0)
                                    model = model.mid(geforce + 8);
                                else if (model.startsWith("NVIDIA "))
                                    model = model.mid(7);
                                gpuModel = model;
                                break;
                            }
                        }
                        break;
                    }
                }
            } else {
                // AMD / Intel: try driver name from uevent
                for (int n = 0; n <= 9 && gpuModel.isEmpty(); ++n) {
                    QFile uevent(QString("/sys/class/drm/card%1/device/uevent").arg(n));
                    if (uevent.open(QIODevice::ReadOnly)) {
                        QString data = QString::fromUtf8(uevent.readAll());
                        for (const QString& line : data.split('\n')) {
                            if (line.startsWith("DRIVER=")) {
                                QString drv = line.mid(7).trimmed();
                                if (drv == "amdgpu") gpuModel = "Radeon";
                                else if (drv == "radeon") gpuModel = "Radeon";
                                else if (drv == "i915") gpuModel = "Intel Graphics";
                                else if (drv == "xe") gpuModel = "Intel Arc";
                                else gpuModel = drv;
                                break;
                            }
                        }
                    }
                }
            }
        }
        profile.gpuModel = gpuModel;

        // NVIDIA driver version from /proc
        QFile nv("/proc/driver/nvidia/version");
        if (nv.open(QIODevice::ReadOnly)) {
            QString data = QString::fromUtf8(nv.readAll());
            for (const QString& line : data.split('\n')) {
                if (line.contains("NVRM version:")) {
                    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                    for (const QString& p : parts) {
                        if (p.contains('.') && p[0].isDigit()) {
                            profile.gpuDriver = p;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    // ── Session / desktop ───────────────────────────────────────────
    {
        QByteArray envVal = qgetenv("XDG_SESSION_TYPE");
        if (!envVal.isEmpty())
            profile.sessionType = QString::fromUtf8(envVal);

        envVal = qgetenv("XDG_CURRENT_DESKTOP");
        if (!envVal.isEmpty())
            profile.desktop = QString::fromUtf8(envVal);
        else {
            envVal = qgetenv("DESKTOP_SESSION");
            if (!envVal.isEmpty())
                profile.desktop = QString::fromUtf8(envVal);
        }
    }

    // Raw metadata
    profile.raw.insert(QStringLiteral("goos"), QStringLiteral("linux"));
#if defined(__x86_64__) || defined(_M_X64)
    profile.raw.insert(QStringLiteral("goarch"), QStringLiteral("amd64"));
#elif defined(__aarch64__) || defined(_M_ARM64)
    profile.raw.insert(QStringLiteral("goarch"), QStringLiteral("arm64"));
#else
    profile.raw.insert(QStringLiteral("goarch"), QStringLiteral("unknown"));
#endif

    // ── Normalize ──────────────────────────────────────────────────────
    normalizeSystemProfile(profile);

    return profile;
}

} // namespace ProtonSage
