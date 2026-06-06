#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <windows.h>
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

    init_apartment(apartment_type::single_threaded);

    DispatcherQueueController dispatcherQueueController{ nullptr };
    if (!DispatcherQueue::GetForCurrentThread()) {
        dispatcherQueueController = DispatcherQueueController::CreateOnCurrentThread();
    }

    Application::Start([](auto&&) {
        make<App>();
    });

    return 0;
}
