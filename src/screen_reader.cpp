#include "../include/screen_reader.h"

#ifdef ENGINE_SIM_SPEECH

#include <prism.h>

#include <chrono>
#include <vector>

namespace {
    // Backends that reach a screen reader, which speaks and brailles the text
    // with the user's own settings. UIA covers Narrator and other screen
    // readers that announce UI Automation notifications.
    const PrismBackendId ScreenReaderBackends[] = {
        PRISM_BACKEND_NVDA,
        PRISM_BACKEND_JAWS,
        PRISM_BACKEND_ZDSR,
        PRISM_BACKEND_ZOOM_TEXT,
        PRISM_BACKEND_SYSTEM_ACCESS,
        PRISM_BACKEND_PC_TALKER,
        PRISM_BACKEND_BOY_PC_READER,
        PRISM_BACKEND_SENSE_READER,
        PRISM_BACKEND_WINDOW_EYES,
        PRISM_BACKEND_VOICE_OVER,
        PRISM_BACKEND_ORCA,
        PRISM_BACKEND_UIA
    };

    // Text to speech without a screen reader
    const PrismBackendId SystemVoiceBackends[] = {
        PRISM_BACKEND_ONE_CORE,
        PRISM_BACKEND_SAPI,
        PRISM_BACKEND_AV_SPEECH,
        PRISM_BACKEND_SPEECH_DISPATCHER
    };

    constexpr std::chrono::seconds RecheckInterval(2);

    bool isRunning(PrismBackend *backend) {
        return (prism_backend_get_features(backend) & PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0;
    }
}

struct ScreenReader::Impl {
    struct Backend {
        PrismBackend *backend = nullptr;
        bool initialized = false;
    };

    PrismContext *context = nullptr;
    std::vector<Backend> screenReaders;
    Backend systemVoice;
    bool systemVoiceSearched = false;

    int current = -1;
    bool checked = false;
    std::chrono::steady_clock::time_point lastCheck;

    static bool ensureInitialized(Backend &b) {
        if (!b.initialized) {
            const PrismError error = prism_backend_initialize(b.backend);
            b.initialized = (error == PRISM_OK || error == PRISM_ERROR_ALREADY_INITIALIZED);
        }

        return b.initialized;
    }

    void refresh() {
        const auto now = std::chrono::steady_clock::now();
        if (checked && now - lastCheck < RecheckInterval) return;

        checked = true;
        lastCheck = now;

        current = -1;
        for (int i = 0; i < static_cast<int>(screenReaders.size()); ++i) {
            if (isRunning(screenReaders[i].backend) && ensureInitialized(screenReaders[i])) {
                current = i;
                break;
            }
        }
    }

    PrismBackend *getSystemVoice() {
        if (!systemVoiceSearched) {
            systemVoiceSearched = true;

            for (const PrismBackendId id : SystemVoiceBackends) {
                if (!prism_registry_exists(context, id)) continue;

                Backend candidate;
                candidate.backend = prism_registry_create(context, id);
                if (candidate.backend == nullptr) continue;

                if (isRunning(candidate.backend) && ensureInitialized(candidate)) {
                    systemVoice = candidate;
                    break;
                }

                prism_backend_free(candidate.backend);
            }
        }

        return systemVoice.backend;
    }

    static bool send(PrismBackend *backend, const std::string &text, bool interrupt) {
        PrismError error = prism_backend_output(backend, text.c_str(), interrupt);
        if (error == PRISM_ERROR_NOT_IMPLEMENTED) {
            error = prism_backend_speak(backend, text.c_str(), interrupt);
        }

        return error == PRISM_OK;
    }
};

ScreenReader::ScreenReader() {
    m_impl = nullptr;
}

ScreenReader::~ScreenReader() {
    destroy();
}

bool ScreenReader::isSupported() {
    return true;
}

void ScreenReader::initialize() {
    if (m_impl != nullptr) return;

    PrismConfig config = prism_config_init();
    PrismContext *context = prism_init(&config);
    if (context == nullptr) return;

    m_impl = new Impl;
    m_impl->context = context;

    for (const PrismBackendId id : ScreenReaderBackends) {
        if (!prism_registry_exists(context, id)) continue;

        Impl::Backend backend;
        backend.backend = prism_registry_create(context, id);
        if (backend.backend != nullptr) {
            m_impl->screenReaders.push_back(backend);
        }
    }
}

void ScreenReader::destroy() {
    if (m_impl == nullptr) return;

    for (const Impl::Backend &backend : m_impl->screenReaders) {
        prism_backend_free(backend.backend);
    }

    if (m_impl->systemVoice.backend != nullptr) {
        prism_backend_free(m_impl->systemVoice.backend);
    }

    prism_shutdown(m_impl->context);

    delete m_impl;
    m_impl = nullptr;
}

bool ScreenReader::isScreenReaderRunning() {
    if (m_impl == nullptr) return false;

    m_impl->refresh();
    return m_impl->current >= 0;
}

bool ScreenReader::output(const std::string &text, bool interrupt, bool allowSystemVoice) {
    if (m_impl == nullptr || text.empty()) return false;

    m_impl->refresh();
    if (m_impl->current >= 0) {
        return Impl::send(m_impl->screenReaders[m_impl->current].backend, text, interrupt);
    }

    if (allowSystemVoice) {
        PrismBackend *voice = m_impl->getSystemVoice();
        if (voice != nullptr) {
            return prism_backend_speak(voice, text.c_str(), interrupt) == PRISM_OK;
        }
    }

    return false;
}

void ScreenReader::stop() {
    if (m_impl == nullptr) return;

    if (m_impl->current >= 0) {
        (void)prism_backend_stop(m_impl->screenReaders[m_impl->current].backend);
    }

    if (m_impl->systemVoice.backend != nullptr) {
        (void)prism_backend_stop(m_impl->systemVoice.backend);
    }
}

std::string ScreenReader::getBackendName() {
    if (m_impl == nullptr) return "";

    m_impl->refresh();
    if (m_impl->current >= 0) {
        return prism_backend_name(m_impl->screenReaders[m_impl->current].backend);
    }
    else if (m_impl->systemVoice.backend != nullptr) {
        return prism_backend_name(m_impl->systemVoice.backend);
    }

    return "";
}

#else /* ENGINE_SIM_SPEECH */

struct ScreenReader::Impl {};

ScreenReader::ScreenReader() {
    m_impl = nullptr;
}

ScreenReader::~ScreenReader() {
    /* void */
}

bool ScreenReader::isSupported() {
    return false;
}

void ScreenReader::initialize() {
    /* void */
}

void ScreenReader::destroy() {
    /* void */
}

bool ScreenReader::isScreenReaderRunning() {
    return false;
}

bool ScreenReader::output(const std::string &, bool, bool) {
    return false;
}

void ScreenReader::stop() {
    /* void */
}

std::string ScreenReader::getBackendName() {
    return "";
}

#endif /* ENGINE_SIM_SPEECH */
