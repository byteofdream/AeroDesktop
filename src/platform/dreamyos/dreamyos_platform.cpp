#include "aero/platform/platform.h"

#include "aero/core/logging.h"
#include "../backend_factory.h"

namespace Aero::Platform {

/// The DreamyOS backend is intentionally a separate implementation boundary.
///
/// It is not implemented in this repository and must not pretend to be.  This
/// factory returns an honest, empty platform: no services, every capability
/// unavailable.  A DreamyOS integration provides implementations of the
/// interfaces in include/aero/platform and installs them here.  See
/// docs/DREAMYOS_INTEGRATION.md.
std::unique_ptr<Platform> createDreamyOSPlatform(QObject* parent)
{
    qCWarning(aeroLog) << "DreamyOS platform backend is a scaffold; no services are installed."
                       << "Aero Desktop requires a DreamyOS service implementation.";
    return std::make_unique<Platform>(Backend::DreamyOS, parent);
}

} // namespace Aero::Platform
