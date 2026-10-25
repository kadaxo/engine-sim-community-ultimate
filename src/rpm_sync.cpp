#include "../include/rpm_sync.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;

bool startNetworking() {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

void stopNetworking() {
    WSACleanup();
}

void closeSocket(NativeSocket s) {
    closesocket(s);
}

bool setNonBlocking(NativeSocket s) {
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}
#else
using NativeSocket = int;

bool startNetworking() {
    return true;
}

void stopNetworking() {
    /* void */
}

void closeSocket(NativeSocket s) {
    close(s);
}

bool setNonBlocking(NativeSocket s) {
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

NativeSocket toNative(std::intptr_t s) {
    return static_cast<NativeSocket>(s);
}
}

RpmSync::RpmSync() {
    m_socket = -1;
    m_networkStarted = false;
    m_port = 8765;
    m_targetRpm = 0.0;
    m_lastUpdate = std::chrono::steady_clock::time_point{};
}

RpmSync::~RpmSync() {
    destroy();
}

bool RpmSync::initialize(int port) {
    destroy();

    m_port = port;
    if (!startNetworking()) {
        return false;
    }
    m_networkStarted = true;

    // INVALID_SOCKET on Windows and -1 elsewhere both map to -1 here
    m_socket = static_cast<std::intptr_t>(socket(AF_INET, SOCK_DGRAM, 0));
    if (m_socket < 0) {
        destroy();
        return false;
    }

    // A blocking socket would stall the UI thread in update()
    if (!setNonBlocking(toNative(m_socket))) {
        destroy();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(m_port));

    if (bind(toNative(m_socket), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        destroy();
        return false;
    }

    return true;
}

void RpmSync::destroy() {
    if (m_socket >= 0) {
        closeSocket(toNative(m_socket));
        m_socket = -1;
    }

    if (m_networkStarted) {
        stopNetworking();
        m_networkStarted = false;
    }
}

void RpmSync::update() {
    if (m_socket < 0) {
        return;
    }

    char buffer[128];
    while (true) {
        // Would-block, errors and empty datagrams all end this poll
        const auto bytes = recv(toNative(m_socket), buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;
        }

        buffer[bytes] = '\0';
        char *end = nullptr;
        const double rpm = std::strtod(buffer, &end);
        if (end != buffer) {
            m_targetRpm = std::clamp(rpm, 0.0, 20000.0);
            m_lastUpdate = std::chrono::steady_clock::now();
        }
    }
}

bool RpmSync::hasTarget() const {
    if (m_lastUpdate.time_since_epoch().count() == 0) {
        return false;
    }

    const auto age = std::chrono::steady_clock::now() - m_lastUpdate;
    return age < std::chrono::milliseconds(250);
}
