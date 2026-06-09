#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include "core/models.h"
#include "steam/vdf.h"

namespace ProtonSage {

/// Reads steamapps/libraryfolders.vdf and returns all library root paths.
/// Always includes the primary root.  Secondary library paths come from the
/// "libraryfolders" object in the VDF.  Returns [root] if the VDF is absent
/// but steamapps/ exists.
QStringList libraryFoldersFromRoot(const QString& root);

/// Scans a Steam root read-only and returns installed games from app manifests
/// across all libraries.  Launch options from localconfig.vdf are merged in.
QList<Game> scanRoot(const QString& root);

/// Scans one Steam library root for appmanifest_*.acf files.
QList<Game> scanLibrary(const QString& libraryPath);

/// Parses one appmanifest_*.acf file and returns the Game struct.
Game parseAppManifestFile(const QString& path, const QString& libraryPath);

/// Converts a parsed AppState VDF object to a Game.
Game parseAppManifest(const VDFObject& obj, const QString& libraryPath);

/// Reads userdata/*/config/localconfig.vdf files and returns per-app LaunchOptions.
/// First-write-first-kept semantics: the first user's value for an appid sticks.
QMap<int, QString> launchOptionsFromRoot(const QString& root);

/// Parses one localconfig.vdf file and returns per-app LaunchOptions.
QMap<int, QString> parseLocalConfigLaunchOptionsFile(const QString& path);

/// Extracts per-app LaunchOptions from a parsed localconfig VDF object.
QMap<int, QString> parseLocalConfigLaunchOptions(const VDFObject& obj);

} // namespace ProtonSage
