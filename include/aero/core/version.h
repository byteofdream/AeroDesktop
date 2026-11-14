#pragma once

#include <QString>

namespace Aero::Core {

/// Semantic version of the Aero Desktop shell.  Kept in a header (rather than
/// generated) so downstream modules and platform backends can use it without
/// depending on the build system.
inline constexpr int VersionMajor = 0;
inline constexpr int VersionMinor = 1;
inline constexpr int VersionPatch = 0;
inline constexpr const char* VersionString = "0.1.0";
inline constexpr const char* CodeName = "Aurora";

inline QString version() { return QString::fromLatin1(VersionString); }

} // namespace Aero::Core
