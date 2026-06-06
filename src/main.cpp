#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <windows.h>
#include <WindowsAppSDK-VersionInfo.h>
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
    HRESULT hr = MddBootstrapInitialize(WINDOWSAPPSDK_RELEASE_MAJORMINOR, WINDOWSAPPSDK_RELEASE_VERSION_SHORTTAG_W, WINDOWSAPPSDK_RUNTIME_VERSION_UINT64);
    if (FAILED(hr))
    {
        return 1;
    }

    init_apartment(apartment_type::single_threaded);

    DispatcherQueueController dispatcherQueueController{ nullptr };
    if (!DispatcherQueue::GetForCurrentThread()) {
        dispatcherQueueController = DispatcherQueueController::CreateOnCurrentThread();
    }

    Application::Start([](auto&&) {
        make<App>();
    });

    MddBootstrapShutdown();
    return 0;
}
