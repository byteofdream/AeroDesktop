#pragma once

#include "aero/platform/platform.h"

#include <memory>

namespace Aero::Platform {

/// Defined by the selected backend translation unit.
std::unique_ptr<Platform> createLinuxPlatform(QObject* parent, Backend backendTag);
std::unique_ptr<Platform> createDreamyOSPlatform(QObject* parent);

} // namespace Aero::Platform
