#pragma once

#include "aero/platform/platform.h"

#include <memory>

namespace Aero::Platform {

std::unique_ptr<Platform> createLinuxPlatform(QObject* parent);
std::unique_ptr<Platform> createDreamyOSPlatform(QObject* parent);

} // namespace Aero::Platform
