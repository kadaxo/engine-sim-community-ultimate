#ifndef ATG_ENGINE_SIM_SCREEN_READER_H
#define ATG_ENGINE_SIM_SCREEN_READER_H

#include <string>

// Speech and braille output through the user's screen reader (NVDA, JAWS,
// Narrator, ...) using the Prism library. Builds without Prism
// (ENGINE_SIM_SPEECH undefined) get an implementation that does nothing.
class ScreenReader {
public:
    ScreenReader();
    ~ScreenReader();

    ScreenReader(const ScreenReader &) = delete;
    ScreenReader &operator=(const ScreenReader &) = delete;

    static bool isSupported();

    void initialize();
    void destroy();

    // Whether a screen reader is running. Rechecked every few seconds, so a
    // screen reader started or closed after the application is noticed.
    bool isScreenReaderRunning();

    // Sends text to the screen reader (speech and braille). Without one,
    // speaks it with the system voice if allowSystemVoice is set. Returns
    // whether anything accepted the text.
    bool output(const std::string &text, bool interrupt, bool allowSystemVoice);

    void stop();

    // Name of the backend in use, e.g. "NVDA", or an empty string
    std::string getBackendName();

private:
    struct Impl;
    Impl *m_impl;
};

#endif /* ATG_ENGINE_SIM_SCREEN_READER_H */
