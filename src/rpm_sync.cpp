#include "../include/rpm_sync.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

RpmSync::RpmSync() {
    m_socket = -1;
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
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        return false;
    }

    const int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(m_port));

    if (bind(m_socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        destroy();
        return false;
    }

    return true;
}

void RpmSync::destroy() {
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
}

void RpmSync::update() {
    if (m_socket < 0) {
        return;
    }

    char buffer[128];
    while (true) {
        const ssize_t bytes = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            break;
        }

        if (bytes == 0) {
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
