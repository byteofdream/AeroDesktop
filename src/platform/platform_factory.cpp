#include "aero/platform/platform.h"

#include "backend_factory.h"

#include <memory>

namespace Aero::Platform {

std::unique_ptr<Platform> createPlatform(QObject* parent, Backend backend)
{
    switch (backend) {
    case Backend::LinuxDev:
        return createLinuxPlatform(parent);
    case Backend::DreamyOS:
        return createDreamyOSPlatform(parent);
    }
    return nullptr;
}

} // namespace Aero::Platform
