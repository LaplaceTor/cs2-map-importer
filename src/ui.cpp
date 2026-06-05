#include "ui.h"
#include "appcore.h"
#include "mapimporter.h"
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <microsoft.ui.xaml.window.interop.h>
#include <shobjidl_core.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <codecvt>
#include <locale>
#include <algorithm>
#include <filesystem>
#include <Windows.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Storage::Pickers;

// Simple string conversions
std::wstring s2ws(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string ws2s(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

ImporterApp::ImporterApp() :
    m_javaInstalled(false),
    m_vpkSignaturesMoved(false),
    m_s1GameType(L"csgo")
{
    std::vector<wchar_t> pathBuf(MAX_PATH);
    GetModuleFileNameW(NULL, pathBuf.data(), MAX_PATH);
    std::filesystem::path exePath(pathBuf.data());
    m_appDir = exePath.parent_path().wstring();

    m_javaInstalled = AppCore::check_java();
}

void ImporterApp::OnLaunched(LaunchActivatedEventArgs const&)
{
    m_window = Window();
    m_window.Title(L"CS2 Map Importer");
    m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

    InitUI();
    BuildUI();

    load_from_cfg();

    // Set up window closing event to cleanup and restore signatures if needed
    m_window.Closed([this](auto&&, auto&&) {
        AppCore::cancel_all();
        if (m_vpkSignaturesMoved && !m_cs2BaseFolder.empty()) {
            AppCore::restore_vpk_signatures(ws2s(m_cs2BaseFolder));
            m_vpkSignaturesMoved = false;
        }
    });

    m_window.Activate();
}

void ImporterApp::InitUI()
{
    m_cs2Label = TextBlock();
    m_cs2Label.Text(L"Not selected");
    m_cs2Label.TextAlignment(TextAlignment::Center);

    m_s1Label = TextBlock();
    m_s1Label.Text(L"Not selected");
    m_s1Label.TextAlignment(TextAlignment::Center);

    m_mapLabel = TextBlock();
    m_mapLabel.Text(L"Not selected");
    m_mapLabel.TextAlignment(TextAlignment::Center);

    m_s1GameCombo = ComboBox();
    m_s1GameCombo.Items().Append(box_value(L"CSGO"));
    m_s1GameCombo.Items().Append(box_value(L"CSS"));
    m_s1GameCombo.SelectedIndex(0);

    m_addonEdit = TextBox();
    m_addonEdit.PlaceholderText(L"Enter addon name:");

    m_useBspCheckBox = CheckBox();
    m_useBspCheckBox.Content(box_value(L"clean unecessary faces in source 2 way"));
    m_useBspCheckBox.IsChecked(true);

    m_noMergeInstancesCheckBox = CheckBox();
    m_noMergeInstancesCheckBox.Content(box_value(L"keep instances"));

    m_skipDepsCheckBox = CheckBox();
    m_skipDepsCheckBox.Content(box_value(L"Skip references import"));

    m_goButton = Button();
    m_goButton.Content(box_value(L"GO!"));
    // m_goButton.Background(SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 0, 255, 0))); // Requires additional includes, keep it simple for now

    m_logOutput = TextBox();
    m_logOutput.IsReadOnly(true);
    m_logOutput.TextWrapping(TextWrapping::Wrap);
    m_logOutput.AcceptsReturn(true);
    m_logOutput.MinHeight(320);
    // Dark theme for log output
    // m_logOutput.Background(SolidColorBrush(Windows::UI::Colors::Black()));
    // m_logOutput.Foreground(SolidColorBrush(Windows::UI::Colors::White()));

    // Wire events
    m_useBspCheckBox.Checked([this](auto&&, auto&&) { on_usebsp_toggled(); });
    m_useBspCheckBox.Unchecked([this](auto&&, auto&&) { on_usebsp_toggled(); });

    m_noMergeInstancesCheckBox.Checked([this](auto&&, auto&&) { on_usebsp_nomergeinstances_toggled(); });
    m_noMergeInstancesCheckBox.Unchecked([this](auto&&, auto&&) { on_usebsp_nomergeinstances_toggled(); });

    m_goButton.Click([this](auto&&, auto&&) { go(); });

    // Store buttons temporarily to add to layout
    m_window.Content(nullptr); // will set in BuildUI
}

void ImporterApp::BuildUI()
{
    StackPanel mainPanel;
    mainPanel.Padding(ThicknessHelper::FromLengths(10, 10, 10, 10));
    mainPanel.Spacing(10);

    // Row 0
    StackPanel row0;
    row0.Orientation(Orientation::Horizontal);
    row0.Spacing(10);
    Button cs2Btn; cs2Btn.Content(box_value(L"Select CS2 folder"));
    cs2Btn.Click([this](auto&&, auto&&) { select_cs2_folder(); });
    Button valCs2Btn; valCs2Btn.Content(box_value(L"Validate CS2"));
    valCs2Btn.Click([this](auto&&, auto&&) { validate_cs2(); });
    row0.Children().Append(cs2Btn);
    row0.Children().Append(m_cs2Label);
    row0.Children().Append(valCs2Btn);
    mainPanel.Children().Append(row0);

    // Row 1
    StackPanel row1;
    row1.Orientation(Orientation::Horizontal);
    row1.Spacing(10);
    Button s1Btn; s1Btn.Content(box_value(L"Select Source 1 game folder"));
    s1Btn.Click([this](auto&&, auto&&) { select_s1_folder(); });
    Button valS1Btn; valS1Btn.Content(box_value(L"Validate Source 1 Game"));
    valS1Btn.Click([this](auto&&, auto&&) { validate_s1(); });
    row1.Children().Append(m_s1GameCombo);
    row1.Children().Append(s1Btn);
    row1.Children().Append(m_s1Label);
    row1.Children().Append(valS1Btn);
    mainPanel.Children().Append(row1);

    // Row 2
    StackPanel row2;
    row2.Orientation(Orientation::Horizontal);
    row2.Spacing(10);
    Button vmfBtn; vmfBtn.Content(box_value(L"Select VMF"));
    vmfBtn.Click([this](auto&&, auto&&) { select_vmf(); });
    Button bspBtn; bspBtn.Content(box_value(L"Select BSP"));
    bspBtn.Click([this](auto&&, auto&&) { select_bsp(); });
    row2.Children().Append(vmfBtn);
    row2.Children().Append(bspBtn);
    row2.Children().Append(m_mapLabel);
    mainPanel.Children().Append(row2);

    // Separator line can be simulated with a rectangle, skip for simplicity or just use empty space

    // Row 5
    mainPanel.Children().Append(m_addonEdit);

    // Row 6
    StackPanel row6;
    row6.Orientation(Orientation::Horizontal);
    row6.Spacing(10);
    row6.Children().Append(m_useBspCheckBox);
    row6.Children().Append(m_noMergeInstancesCheckBox);
    row6.Children().Append(m_goButton);
    mainPanel.Children().Append(row6);

    // Row 7
    mainPanel.Children().Append(m_skipDepsCheckBox);

    // Row 9 (Log)
    ScrollViewer scroll;
    scroll.Content(m_logOutput);
    mainPanel.Children().Append(scroll);

    m_window.Content(mainPanel);
}

// Dialog Wrappers via Windows App SDK pattern
std::wstring ImporterApp::SelectFolderDialog(const std::wstring& title)
{
    HWND hwnd;
    m_window.as<IWindowNative>()->get_WindowHandle(&hwnd);
    // We cannot easily block on async in UI thread in WinRT without deadlock,
    // but CppWinRT get() blocks. Alternatively we should use coroutines.
    // For simplicity of conversion, we'll try blocking, but normally this needs a co_await.
    // However, WinUI3 strictly requires async handling for pickers on the UI thread.
    // Since we're refactoring, let's use a simpler approach: fallback to Win32 standard dialogs to avoid coroutine refactor of the whole UI.

    IFileOpenDialog *pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr))
    {
        pFileOpen->SetOptions(FOS_PICKFOLDERS);
        pFileOpen->SetTitle(title.c_str());
        hr = pFileOpen->Show(hwnd);
        if (SUCCEEDED(hr))
        {
            IShellItem *pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr))
            {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr))
                {
                    std::wstring res = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                    pItem->Release();
                    pFileOpen->Release();
                    return res;
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return L"";
}

std::wstring ImporterApp::SelectFileDialog(const std::wstring& title, const std::wstring& filterName, const std::wstring& filterExt)
{
    HWND hwnd;
    m_window.as<IWindowNative>()->get_WindowHandle(&hwnd);

    IFileOpenDialog *pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr))
    {
        pFileOpen->SetTitle(title.c_str());
        COMDLG_FILTERSPEC rgSpec[] = { { filterName.c_str(), filterExt.c_str() } };
        pFileOpen->SetFileTypes(1, rgSpec);
        hr = pFileOpen->Show(hwnd);
        if (SUCCEEDED(hr))
        {
            IShellItem *pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr))
            {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr))
                {
                    std::wstring res = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                    pItem->Release();
                    pFileOpen->Release();
                    return res;
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
    return L"";
}

void ImporterApp::set_cs2_folder(const std::wstring& path)
{
    m_cs2BaseFolder = path;
    m_cs2Label.Text(path);
}

void ImporterApp::set_s1_folder(const std::wstring& path)
{
    m_s1GameBaseFolder = path;
    m_s1Label.Text(path);
}

void ImporterApp::select_cs2_folder()
{
    std::wstring path = SelectFolderDialog(L"Select your Counter-Strike 2 game folder");
    if (!path.empty()) {
        set_cs2_folder(path);
    }
}

void ImporterApp::select_s1_folder()
{
    std::wstring path = SelectFolderDialog(L"Select your Source 1 game folder (e.g., csgo legacy)");
    if (!path.empty()) {
        set_s1_folder(path);

        auto comboVal = winrt::unbox_value<winrt::hstring>(m_s1GameCombo.SelectedItem());
        if (comboVal == L"CSS") {
            m_s1GameType = L"css";
        } else {
            m_s1GameType = L"csgo";
        }
    }
}

void ImporterApp::select_vmf()
{
    std::wstring path = SelectFileDialog(L"Select a VMF", L"VMF files", L"*.vmf");
    if (path.empty()) return;

    m_bspFile.clear();

    std::filesystem::path fp(path);
    std::wstring vmf_name = fp.stem().wstring();

    // Process copy into app isolated folder
    std::filesystem::path target_dir = std::filesystem::path(m_appDir) / L"maps" / vmf_name / L"maps";
    std::filesystem::path target_path = target_dir / fp.filename();

    std::error_code ec;
    std::filesystem::create_directories(target_dir, ec);
    std::filesystem::copy_file(fp, target_path, std::filesystem::copy_options::overwrite_existing, ec);

    m_contentFolder = target_dir.parent_path().wstring();
    m_mapName = vmf_name;
    m_contentFolderToSave = m_contentFolder;

    log(L"VMF set up at: " + target_path.wstring());

    m_mapLabel.Text(L"Selected");
}

void ImporterApp::select_bsp()
{
    std::wstring path = SelectFileDialog(L"Select a BSP", L"BSP files", L"*.bsp");
    if (path.empty()) return;

    m_bspFile = path;
    std::filesystem::path fp(path);
    m_mapName = fp.stem().wstring();
    m_contentFolderToSave = fp.parent_path().wstring();

    m_mapLabel.Text(L"Selected");
}

void ImporterApp::validate_cs2()
{
    if (m_cs2BaseFolder.empty()) {
        log(L"CS2 folder not selected.");
        return;
    }
    std::filesystem::path p(m_cs2BaseFolder);
    if (std::filesystem::exists(p / L"game" / L"csgo" / L"gameinfo.gi")) {
        log(L"CS2 validation successful.");
    } else {
        log(L"CS2 validation failed: gameinfo.gi not found.");
    }
}

void ImporterApp::validate_s1()
{
    if (m_s1GameBaseFolder.empty()) {
        log(L"Source 1 folder not selected.");
        return;
    }

    auto comboVal = winrt::unbox_value<winrt::hstring>(m_s1GameCombo.SelectedItem());
    std::wstring expectedSub = (comboVal == L"CSS") ? L"cstrike" : L"csgo";

    std::filesystem::path p(m_s1GameBaseFolder);
    if (std::filesystem::exists(p / expectedSub / L"gameinfo.txt")) {
        log(L"Source 1 validation successful.");
    } else {
        log(L"Source 1 validation failed: gameinfo.txt not found in " + expectedSub);
    }
}

void ImporterApp::on_usebsp_toggled()
{
    bool checked = false;
    if (m_useBspCheckBox.IsChecked()) {
        checked = m_useBspCheckBox.IsChecked().Value();
    }
    m_noMergeInstancesCheckBox.IsEnabled(checked);
    if (!checked) {
        m_noMergeInstancesCheckBox.IsChecked(false);
    }
}

void ImporterApp::on_usebsp_nomergeinstances_toggled()
{
    bool checked = false;
    if (m_noMergeInstancesCheckBox.IsChecked()) {
        checked = m_noMergeInstancesCheckBox.IsChecked().Value();
    }
    if (checked) {
        m_useBspCheckBox.IsChecked(true);
    }
}

void ImporterApp::get_addon_name()
{
    m_addonName = m_addonEdit.Text().c_str();
}

void ImporterApp::get_launch_options()
{
    // Keeping logic similar
}

void ImporterApp::load_from_cfg()
{
    std::ifstream file("cs2importer.cfg");
    if (!file.is_open()) return;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    if (lines.empty()) return;

    if (lines[0] == "True" || lines[0] == "False") {
        if (lines.size() >= 6) {
            m_useBspCheckBox.IsChecked(lines[0] == "True");
            m_noMergeInstancesCheckBox.IsChecked(lines[1] == "True");
            m_noMergeInstancesCheckBox.IsEnabled(lines[0] == "True");
            m_skipDepsCheckBox.IsChecked(lines[2] == "True");
            set_cs2_folder(s2ws(lines[3]));

            if (lines.size() >= 7) {
                m_s1GameType = s2ws(lines[6]);
            } else {
                m_s1GameType = L"csgo";
            }

            if (m_s1GameType == L"css") {
                m_s1GameCombo.SelectedIndex(1);
            } else {
                m_s1GameCombo.SelectedIndex(0);
            }

            set_s1_folder(s2ws(lines[4]));
            m_vmfDefaultPath = s2ws(lines[5]);
        }
    }
}

void ImporterApp::save_to_cfg()
{
    std::ofstream file("cs2importer.cfg");
    if (!file.is_open()) return;

    bool useBspChecked = m_useBspCheckBox.IsChecked() && m_useBspCheckBox.IsChecked().Value();
    bool noMergeChecked = m_noMergeInstancesCheckBox.IsChecked() && m_noMergeInstancesCheckBox.IsChecked().Value();
    bool skipDepsChecked = m_skipDepsCheckBox.IsChecked() && m_skipDepsCheckBox.IsChecked().Value();

    file << (useBspChecked ? "True" : "False") << "\n";
    file << (noMergeChecked ? "True" : "False") << "\n";
    file << (skipDepsChecked ? "True" : "False") << "\n";
    file << ws2s(m_cs2BaseFolder) << "\n";
    file << ws2s(m_s1GameBaseFolder) << "\n";
    file << ws2s(m_contentFolderToSave) << "\n";

    auto comboVal = winrt::unbox_value<winrt::hstring>(m_s1GameCombo.SelectedItem());
    if (comboVal == L"CSS") {
        file << "css\n";
    } else {
        file << "csgo\n";
    }
}

void ImporterApp::log(const std::wstring& message)
{
    std::wstring msg = message + L"\r\n";

    // Update UI on dispatcher
    m_dispatcherQueue.TryEnqueue([this, msg]() {
        m_logOutput.Text(m_logOutput.Text() + msg);
    });

    if (m_logFile.is_open()) {
        m_logFile << ws2s(msg);
        m_logFile.flush();
    }
}

void ImporterApp::log(const std::string& message)
{
    log(s2ws(message));
}

void ImporterApp::go()
{
    m_logOutput.Text(L"");

    if (m_cs2BaseFolder.empty() || m_s1GameBaseFolder.empty()) {
        log(L"Error: CS2 or Source 1 folder not selected.");
        return;
    }
    if (m_bspFile.empty() && m_contentFolder.empty()) {
        log(L"Error: Please select a VMF or BSP file.");
        return;
    }

    try {
        get_addon_name();
        if (m_addonName.empty()) {
            m_addonName = m_mapName;
        }

        save_to_cfg();

        std::filesystem::path logDirPath = std::filesystem::path(m_appDir) / L"log";
        std::error_code ec;
        std::filesystem::create_directories(logDirPath, ec);

        // Simplistic timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[100];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", std::localtime(&t));

        std::wstring logFilename = s2ws(buf) + L"_" + m_addonName + L".log";
        std::filesystem::path logFilePath = logDirPath / logFilename;

        if (m_logFile.is_open()) {
            m_logFile.close();
        }
        m_logFile.open(logFilePath, std::ios::out | std::ios::app);

        AppCore::cancel_import = false;
        AppCore::move_vpk_signatures(ws2s(m_cs2BaseFolder), m_vpkSignaturesMoved);

        m_goButton.IsEnabled(false);
        log(L"Starting AppCore thread...");

        AppCore::Options opts;
        opts.cs2_basefolder = ws2s(m_cs2BaseFolder);
        std::replace(opts.cs2_basefolder.begin(), opts.cs2_basefolder.end(), '/', '\\');

        opts.s1game_basefolder = ws2s(m_s1GameBaseFolder);

        auto comboVal = winrt::unbox_value<winrt::hstring>(m_s1GameCombo.SelectedItem());
        opts.s1_game_type = (comboVal == L"CSS") ? "css" : "csgo";

        opts.content_folder = ws2s(m_contentFolder);
        opts.map_name = ws2s(m_mapName);
        opts.bsp_file = ws2s(m_bspFile);
        opts.app_dir = ws2s(m_appDir);
        opts.addon_name = ws2s(m_addonName);

        bool useBspChecked = m_useBspCheckBox.IsChecked() && m_useBspCheckBox.IsChecked().Value();
        bool noMergeChecked = m_noMergeInstancesCheckBox.IsChecked() && m_noMergeInstancesCheckBox.IsChecked().Value();
        bool skipDepsChecked = m_skipDepsCheckBox.IsChecked() && m_skipDepsCheckBox.IsChecked().Value();

        opts.usebsp = useBspChecked && !noMergeChecked;
        opts.usebsp_nomergeinstances = useBspChecked && noMergeChecked;
        opts.skipdeps = skipDepsChecked;

        opts.logger = [this](const std::string& msg) {
            this->log(msg);
        };

        std::thread([this, opts]() mutable {
            bool success = true;
            try {
                if (!opts.bsp_file.empty()) {
                    if (!AppCore::check_java()) {
                        throw std::runtime_error("Java is not installed. Cannot decompile BSP file.");
                    }
                    AppCore::process_bsp(opts);
                }

                MapImporter::Options mapOpts;
                std::string s1_subfolder = opts.s1_game_type == "css" ? "cstrike" : "csgo";

                std::string s1gamedir = opts.s1game_basefolder + "\\" + s1_subfolder;
                for (size_t i = 0; i < s1gamedir.length(); ++i) if (s1gamedir[i] == '/') s1gamedir[i] = '\\';
                mapOpts.s1gamedir = s1gamedir;

                mapOpts.s1gamename = opts.s1_game_type == "css" ? "css" : "csgo";

                std::string contentdir = opts.content_folder;
                for (size_t i = 0; i < contentdir.length(); ++i) if (contentdir[i] == '/') contentdir[i] = '\\';
                mapOpts.s1contentdir = contentdir;

                mapOpts.s2addonname = opts.addon_name;
                mapOpts.s2contentdir = opts.cs2_basefolder + "\\content\\csgo_addons\\" + opts.addon_name;
                mapOpts.mapname = opts.map_name;
                mapOpts.usebsp = opts.usebsp;
                mapOpts.usebsp_nomergeinstances = opts.usebsp_nomergeinstances;
                mapOpts.skipdeps = opts.skipdeps;
                mapOpts.cs2_basefolder = opts.cs2_basefolder;

                MapImporter importer(mapOpts, opts.logger);
                success = importer.Run();

            } catch (const std::exception& e) {
                opts.logger(std::string("Error: ") + e.what());
                success = false;
            }

            m_dispatcherQueue.TryEnqueue([this, success]() {
                if (m_vpkSignaturesMoved && !m_cs2BaseFolder.empty()) {
                    AppCore::restore_vpk_signatures(ws2s(m_cs2BaseFolder));
                    m_vpkSignaturesMoved = false;
                }
                m_goButton.IsEnabled(true);
                if (success) {
                    log(L"MapImporter thread finished successfully.");
                } else {
                    log(L"MapImporter thread finished with errors.");
                }

                if (m_logFile.is_open()) {
                    m_logFile.close();
                }
            });

        }).detach();

    } catch (const std::exception& e) {
        log(std::string("Error: ") + e.what());
    }
}
