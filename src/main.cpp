#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <windows.h>
#include <MddBootstrap.h>
#include "ui.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Dispatching;

struct App : ApplicationT<App>
{
    void OnLaunched(LaunchActivatedEventArgs const&)
    {
        importer = std::make_unique<Importer>();
        importer->GetWindow().Activate();
    }
private:
    std::unique_ptr<Importer> importer;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    // Initialize the Windows App SDK bootstrap for unpackaged apps
    const UINT32 majorMinorVersion = 0x00010004; // Windows App SDK 1.4
    PCWSTR versionTag = L"";
    const PACKAGE_VERSION minVersion{};
    HRESULT hr = MddBootstrapInitialize2(majorMinorVersion, versionTag, minVersion, MddBootstrapInitializeOptions_OnNoMatch_ShowUI);
    if (FAILED(hr)) {
        MessageBoxA(nullptr, "Failed to initialize Windows App SDK", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    init_apartment(apartment_type::single_threaded);

    auto dispatcherQueueController = DispatcherQueueController::CreateOnCurrentThread();

    Application::Start([](auto&&) {
        make<App>();
    });

    MddBootstrapShutdown();
    return 0;
}
