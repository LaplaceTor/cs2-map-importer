#include <windows.h>
#include <unknwn.h>
#include <winrt/base.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include "ui.h"

// Note: To build properly on MSVC without a console, the entry point for a GUI app is wWinMain.
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize COM on the thread
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // MddBootstrapInitialize is used if the app is unpackaged to hook up to Windows App SDK,
    // but the Microsoft.WindowsAppSDK MSBuild targets generate AutoInitialize logic implicitly
    // when using NuGet packages. The XamlApplication start handles running the event loop.

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
        // App is a subclass of winrt::Microsoft::UI::Xaml::Application
        // Importer class handles the MainWindow logic.
        ::winrt::make<ImporterApp>();
    });

    return 0;
}
