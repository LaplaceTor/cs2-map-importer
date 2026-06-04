#include "pch.h"
#include "App.xaml.h"
#include <winrt/Microsoft.UI.Xaml.h>

using namespace winrt;

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    init_apartment(apartment_type::single_threaded);

    ::winrt::Microsoft::UI::Xaml::Application::Start(
        [](auto&&)
        {
            ::winrt::make<::winrt::cs2importer::implementation::App>();
        });

    return 0;
}
