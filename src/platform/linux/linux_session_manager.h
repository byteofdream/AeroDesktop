#pragma once

#include "aero/platform/i_session_manager.h"

namespace Aero::Platform::Linux {

class LinuxSessionManager final : public ISessionManager {
    Q_OBJECT
public:
    explicit LinuxSessionManager(QObject* parent = nullptr);

    bool isAvailable() const override;
    SessionInfo session() const override;
    UserInfo currentUser() const override;
    QList<UserInfo> users() const override;
    SystemInfo systemInfo() const override;

    bool lockSession() override;
    bool logoutSession() override;
    void refresh() override;

private:
    void readSystemInfo();
    void readUsers();

    SessionInfo m_session;
    UserInfo m_currentUser;
    QList<UserInfo> m_users;
    SystemInfo m_systemInfo;
    bool m_logind = false;
};

} // namespace Aero::Platform::Linux
