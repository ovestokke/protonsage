#include "helpers.h"
#include <QCryptographicHash>

namespace ProtonSage {

bool isEnvAssignment(const QString& token) {
    int eq = token.indexOf('=');
    if (eq <= 0) return false;
    QString n = token.left(eq).toUpper();
    return n.startsWith("PROTON_") || n.startsWith("DXVK_") || n.startsWith("VKD3D_")
        || n.startsWith("RADV_") || n.startsWith("MESA_") || n.startsWith("WINE")
        || n.startsWith("__GL_") || n.startsWith("NVIDIA_") || n.startsWith("AMD_")
        || n.startsWith("SDL_") || n == "WINEDLLOVERRIDES" || n == "MANGOHUD"
        || n == "ENABLE_VKBASALT" || n == "PULSE_LATENCY_MSEC"
        || n == "LD_PRELOAD" || n == "STEAM_COMPAT" || n == "DXVK_FRAME_RATE"
        || n == "DXVK_HDR" || n == "ENABLE_HDR_WSI" || n == "MANGOHUD_CONFIG";
}

EnvSplit splitEnv(const QString& token) {
    EnvSplit r;
    int eq = token.indexOf('=');
    if (eq <= 0 || eq >= token.size() - 1) return r;
    r.name = token.left(eq).trimmed().toUpper();
    r.value = token.mid(eq + 1).trimmed();
    r.ok = !r.name.isEmpty();
    return r;
}

QString trimOuterQuotes(const QString& s) {
    QString t = s.trimmed();
    if (t.size() >= 2 && ((t[0] == '"' && t[t.size()-1] == '"') ||
                           (t[0] == '\'' && t[t.size()-1] == '\'')))
        return t.mid(1, t.size() - 2);
    return t;
}

bool isDangerous(const QString& snippet) {
    for (const QChar& c : snippet) {
        if (c.unicode() < 32 && c != '\n' && c != '\r' && c != '\t') return true;
    }
    QString l = snippet.toLower().trimmed();
    for (const QString& w : {"sudo", "rm ", "rmdir", "mkfs", "dd if=", "chmod", "chown",
                              "pkexec", "wget ", "curl "}) {
        if (l.startsWith(w) || l.contains(" " + w)) return true;
    }
    return snippet.contains("$(") || snippet.contains("`") || snippet.contains("&&")
        || snippet.contains("||") || snippet.contains(";|");
}

bool isWrapperCmd(const QString& token) {
    static const QStringList w = {"gamemoderun","mangohud","gamescope","prime-run","obs-gamecapture","game-performance","dlss-swapper"};
    return w.contains(token.toLower());
}

bool isCopyableKind(const QString& kind) {
    return kind == "launch_option" || kind == "env_var" || kind == "wrapper";
}

} // namespace ProtonSage
