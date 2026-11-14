#pragma once

#include "aero/platform/platform_types.h"
#include "aero/platform/platform_service.h"

namespace Aero::Platform {

class ISessionManager : public PlatformService {
    Q_OBJECT
public:
    explicit ISessionManager(QObject* parent = nullptr)
        : PlatformService(parent)
    {
        setCapability(Capability::UserSessions);
    }

    virtual SessionInfo session() const = 0;
    virtual UserInfo currentUser() const = 0;
    virtual QList<UserInfo> users() const = 0;
    virtual SystemInfo systemInfo() const = 0;

    virtual bool lockSession() = 0;

    /// Logging out is intentionally guarded by the shell.  Backends may return
    /// false when they cannot end a session safely.
    virtual bool logoutSession() = 0;

    virtual void refresh() = 0;

signals:
    void sessionChanged();
    void usersChanged();
};

} // namespace Aero::Platform
