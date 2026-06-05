#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Dispatching.h>

class ImporterApp : public winrt::Microsoft::UI::Xaml::ApplicationT<ImporterApp>
{
public:
    ImporterApp();
    void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);

private:
    winrt::Microsoft::UI::Xaml::Window m_window{ nullptr };
    winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

    // UI elements
    winrt::Microsoft::UI::Xaml::Controls::TextBlock m_cs2Label{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock m_s1Label{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock m_mapLabel{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::ComboBox m_s1GameCombo{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBox m_addonEdit{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::CheckBox m_useBspCheckBox{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::CheckBox m_noMergeInstancesCheckBox{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::CheckBox m_skipDepsCheckBox{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Button m_goButton{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBox m_logOutput{ nullptr };

    // App state
    std::wstring m_appDir;
    bool m_javaInstalled;
    std::wstring m_vmfDefaultPath;
    std::wstring m_cs2BaseFolder;
    std::wstring m_s1GameBaseFolder;
    std::wstring m_s1GameType; // "csgo" or "css"
    std::wstring m_contentFolder;
    std::wstring m_contentFolderToSave;
    std::wstring m_addonName;
    std::wstring m_mapName;
    bool m_vpkSignaturesMoved;
    std::wstring m_bspFile;
    std::wstring m_launchOptions;

    std::ofstream m_logFile;

    // Methods
    void InitUI();
    void BuildUI();

    void select_cs2_folder();
    void select_s1_folder();
    void select_vmf();
    void select_bsp();
    void validate_cs2();
    void validate_s1();
    void on_usebsp_toggled();
    void on_usebsp_nomergeinstances_toggled();
    void get_addon_name();
    void get_launch_options();
    void go();

    void set_cs2_folder(const std::wstring& path);
    void set_s1_folder(const std::wstring& path);

    void load_from_cfg();
    void save_to_cfg();

    std::wstring SelectFolderDialog(const std::wstring& title);
    std::wstring SelectFileDialog(const std::wstring& title, const std::wstring& filterName, const std::wstring& filterExt);

public:
    void log(const std::wstring& message);
    void log(const std::string& message);
};
