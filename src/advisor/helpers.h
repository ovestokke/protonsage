#pragma once
#include <QString>
#include <QRegularExpression>

namespace ProtonSage {

// ── Shared internal helpers ──────────────────────────────────────────

struct EnvSplit { QString name, value; bool ok = false; };
bool isEnvAssignment(const QString& token);
EnvSplit splitEnv(const QString& token);
QString trimOuterQuotes(const QString& s);
bool isDangerous(const QString& snippet);
bool isWrapperCmd(const QString& token);
bool isCopyableKind(const QString& kind);

inline double round2(double v) { return qRound(v * 100.0) / 100.0; }

} // namespace ProtonSage
