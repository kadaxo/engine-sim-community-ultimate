#include "../include/engine_sim_application.h"

static int runApp(void *handle, ysContextObject::DeviceAPI api) {
    EngineSimApplication application;
    if (!application.initialize(handle, api)) return 1;
    application.run();
    application.destroy();
    return 0;
}

#if _WIN32
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)nCmdShow;
    (void)lpCmdLine;
    (void)hPrevInstance;

    return runApp(static_cast<void*>(&hInstance), ysContextObject::DeviceAPI::DirectX11);
}

#else
int main() {
    return runApp(nullptr, ysContextObject::DeviceAPI::OpenGL4_0);
}

#endif
