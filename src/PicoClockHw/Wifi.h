#pragma once

#include <string>

class Wifi
{
public:
    enum Status
    {
        Unknown,
        OK,
        NotAvailable,
        Down,
        Connecting,
        NoIp,
        Connected,
        ConnectionFailed,
        NoNetworkFound,
        AuthenticationFailed,
    };

    static bool init();
    static void deinit();
    static bool connectBlocking();
    static bool connectAsync();
    static Status linkStatus();
    static std::string linkStatusToString(Status s);

private:
    static Status m_connectResult;

    static bool handleConnectResult(int res);
};

#ifndef PICO_CYW43_SUPPORTED
inline bool Wifi::init()
{
    return false;
}

inline void Wifi::deinit()
{}

inline bool Wifi::connectBlocking()
{
    return false;
}

inline bool Wifi::connectAsync()
{
    return false;
}

inline Wifi::Status Wifi::linkStatus()
{
    return NotAvailable;
}

inline std::string Wifi::linkStatusToString(Status s)
{
    return "";
}

#endif
