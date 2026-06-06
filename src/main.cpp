#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <windows.h>
#include <WindowsAppSDK-VersionInfo.h>
#include <MddBootstrap.h>
#include "ui.h"

struct App : winrt::Microsoft::UI::Xaml::ApplicationT<App>
{
    void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&)
    {
        importer = std::make_unique<Importer>();
        importer->GetWindow().Activate();
    }
private:
    std::unique_ptr<Importer> importer;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    PACKAGE_VERSION version{};
    version.Version = WINDOWSAPPSDK_RUNTIME_VERSION_UINT64;
    HRESULT hr = MddBootstrapInitialize(WINDOWSAPPSDK_RELEASE_MAJORMINOR, WINDOWSAPPSDK_RELEASE_VERSION_SHORTTAG_W, version);
    if (FAILED(hr))
    {
        return 1;
    }

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    winrt::Microsoft::UI::Dispatching::DispatcherQueueController dispatcherQueueController{ nullptr };
    if (!winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread()) {
        dispatcherQueueController = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();
    }

    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
        winrt::make<App>();
    });

    MddBootstrapShutdown();
    return 0;
}
