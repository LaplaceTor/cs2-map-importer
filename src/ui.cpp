#include "ui.h"
#include "mapimporter.h"
#include "appcore.h"

#include <windows.h>
#include <shobjidl.h>
#include <thread>
#include <filesystem>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <microsoft.ui.xaml.window.h>
#include <algorithm>
#include <cctype>

#undef FindText

namespace fs = std::filesystem;
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;

static bool get_checked(const IReference<bool>& ref) {
    return ref ? ref.Value() : false;
}

static std::string my_to_string(const hstring& hstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, hstr.c_str(), (int)hstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, hstr.c_str(), (int)hstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

static hstring to_hstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return hstring(wstrTo);
}

static void ShowMessage(const std::string& title, const std::string& content) {
    MessageBoxA(NULL, content.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

static void ShowError(const std::string& title, const std::string& content) {
    MessageBoxA(NULL, content.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

Importer::Importer()
    : java_installed(false),
      vmf_default_path("C:\\"),
      content_folder_to_save("C:\\"),
      vpk_signatures_moved(false),
      log_file(nullptr)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    app_dir = fs::path(path).parent_path().string();

    window = Window();
    window.Title(L"CS2 Importer");

    create_ui();

    log("Initializing CS2 Importer...");

    java_installed = AppCore::check_java();

    set_tooltips();
    set_stylesheets();

    get_addon_name();
    get_launch_options();

    if (!java_installed) {
        ToolTipService::SetToolTip(bsp_button, box_value(to_hstring("Java is missing. BSP decompilation is disabled.")));
        bsp_button.IsEnabled(false);
        log("Warning: Java is missing. BSP decompilation disabled.");
    }

    load_from_cfg();

    // Initial state
    usebsp_nomergeinstances_checkbox.IsEnabled(get_checked(usebsp_checkbox.IsChecked()));

    window.Closed([this](IInspectable const&, WindowEventArgs const& args) {
        AppCore::cancel_all();
        if (vpk_signatures_moved && !cs2_basefolder.empty()) {
            AppCore::restore_vpk_signatures(cs2_basefolder);
            vpk_signatures_moved = false;
        }
    });

    log("Initializing CS2 Importer... Finished");
}

Importer::~Importer()
{
    if (log_file) {
        if (log_file->is_open()) {
            log_file->close();
        }
        delete log_file;
        log_file = nullptr;
    }
}

void Importer::create_ui()
{
    StackPanel rootPanel;
    rootPanel.Padding(ThicknessHelper::FromLengths(10, 10, 10, 10));
    rootPanel.Spacing(10);

    // Row 0
    StackPanel row0;
    row0.Orientation(Orientation::Horizontal);
    row0.Spacing(10);

    cs2_button = Button();
    cs2_button.Content(box_value(L"Select CS2 folder"));
    cs2_button.Click([this](IInspectable const&, RoutedEventArgs const&) { select_cs2_folder(); });
    row0.Children().Append(cs2_button);

    cs2_label = TextBlock();
    cs2_label.Text(L"Not selected");
    cs2_label.VerticalAlignment(VerticalAlignment::Center);
    cs2_label.Width(150);
    cs2_label.TextAlignment(TextAlignment::Center);
    row0.Children().Append(cs2_label);

    validate_cs2_button = Button();
    validate_cs2_button.Content(box_value(L"Validate CS2"));
    validate_cs2_button.Click([this](IInspectable const&, RoutedEventArgs const&) { validate_cs2(); });
    row0.Children().Append(validate_cs2_button);

    rootPanel.Children().Append(row0);

    // Row 1
    StackPanel row1;
    row1.Orientation(Orientation::Horizontal);
    row1.Spacing(10);

    s1_game_combo = ComboBox();
    s1_game_combo.Items().Append(box_value(L"CSGO"));
    s1_game_combo.Items().Append(box_value(L"CSS"));
    s1_game_combo.SelectedIndex(0);
    s1_game_combo.SelectionChanged([this](IInspectable const&, SelectionChangedEventArgs const&) {
        s1game_basefolder.clear();
        s1_label.Text(L"Not selected");
        s1_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
    });
    row1.Children().Append(s1_game_combo);

    s1_button = Button();
    s1_button.Content(box_value(L"Select Source 1 game folder"));
    s1_button.Click([this](IInspectable const&, RoutedEventArgs const&) { select_s1_folder(); });
    row1.Children().Append(s1_button);

    s1_label = TextBlock();
    s1_label.Text(L"Not selected");
    s1_label.VerticalAlignment(VerticalAlignment::Center);
    s1_label.Width(150);
    s1_label.TextAlignment(TextAlignment::Center);
    row1.Children().Append(s1_label);

    validate_s1_button = Button();
    validate_s1_button.Content(box_value(L"Validate Source 1 Game"));
    validate_s1_button.Click([this](IInspectable const&, RoutedEventArgs const&) { validate_s1(); });
    row1.Children().Append(validate_s1_button);

    rootPanel.Children().Append(row1);

    // Row 2
    StackPanel row2;
    row2.Orientation(Orientation::Horizontal);
    row2.Spacing(10);

    vmf_button = Button();
    vmf_button.Content(box_value(L"Select VMF"));
    vmf_button.Click([this](IInspectable const&, RoutedEventArgs const&) { select_vmf(); });
    row2.Children().Append(vmf_button);

    bsp_button = Button();
    bsp_button.Content(box_value(L"Select BSP"));
    bsp_button.Click([this](IInspectable const&, RoutedEventArgs const&) { select_bsp(); });
    row2.Children().Append(bsp_button);

    map_label = TextBlock();
    map_label.Text(L"Not selected");
    map_label.VerticalAlignment(VerticalAlignment::Center);
    map_label.Width(150);
    map_label.TextAlignment(TextAlignment::Center);
    row2.Children().Append(map_label);

    rootPanel.Children().Append(row2);

    // Separator (Line equivalent)
    Border separator;
    separator.Height(1);
    separator.Background(SolidColorBrush(Microsoft::UI::Colors::Gray()));
    separator.Margin(ThicknessHelper::FromLengths(0, 5, 0, 5));
    rootPanel.Children().Append(separator);

    // Addon Edit
    addon_edit = TextBox();
    addon_edit.PlaceholderText(L"Enter addon name:");
    addon_edit.TextChanged([this](IInspectable const&, TextChangedEventArgs const&) { get_addon_name(); });
    rootPanel.Children().Append(addon_edit);

    // Row 6
    StackPanel row6;
    row6.Orientation(Orientation::Horizontal);
    row6.Spacing(10);

    usebsp_checkbox = CheckBox();
    usebsp_checkbox.Content(box_value(L"clean unecessary faces in source 2 way"));
    usebsp_checkbox.IsChecked(true);
    usebsp_checkbox.Checked([this](IInspectable const&, RoutedEventArgs const&) { on_usebsp_toggled(true); get_launch_options(); });
    usebsp_checkbox.Unchecked([this](IInspectable const&, RoutedEventArgs const&) { on_usebsp_toggled(false); get_launch_options(); });
    row6.Children().Append(usebsp_checkbox);

    usebsp_nomergeinstances_checkbox = CheckBox();
    usebsp_nomergeinstances_checkbox.Content(box_value(L"keep instances"));
    usebsp_nomergeinstances_checkbox.Checked([this](IInspectable const&, RoutedEventArgs const&) { on_usebsp_nomergeinstances_toggled(true); get_launch_options(); });
    usebsp_nomergeinstances_checkbox.Unchecked([this](IInspectable const&, RoutedEventArgs const&) { on_usebsp_nomergeinstances_toggled(false); get_launch_options(); });
    row6.Children().Append(usebsp_nomergeinstances_checkbox);

    go_button = Button();
    go_button.Content(box_value(L"GO!"));
    go_button.Background(SolidColorBrush(Microsoft::UI::Colors::LimeGreen()));
    go_button.Foreground(SolidColorBrush(Microsoft::UI::Colors::Black()));
    go_button.FontWeight(Microsoft::UI::Text::FontWeights::Bold());
    go_button.Click([this](IInspectable const&, RoutedEventArgs const&) { go(); });
    row6.Children().Append(go_button);

    rootPanel.Children().Append(row6);

    // Row 7
    skipdeps_checkbox = CheckBox();
    skipdeps_checkbox.Content(box_value(L"Skip references import"));
    skipdeps_checkbox.Checked([this](IInspectable const&, RoutedEventArgs const&) { get_launch_options(); });
    skipdeps_checkbox.Unchecked([this](IInspectable const&, RoutedEventArgs const&) { get_launch_options(); });
    rootPanel.Children().Append(skipdeps_checkbox);

    // Log Output
    log_scroll = ScrollViewer();
    log_scroll.Height(320);
    log_scroll.Background(SolidColorBrush(Microsoft::UI::Colors::Black()));

    log_output = TextBlock();
    log_output.Foreground(SolidColorBrush(Microsoft::UI::Colors::White()));
    log_output.TextWrapping(TextWrapping::Wrap);
    log_output.FontFamily(Media::FontFamily(L"Consolas"));
    log_output.Margin(ThicknessHelper::FromLengths(5, 5, 5, 5));
    log_scroll.Content(log_output);

    rootPanel.Children().Append(log_scroll);

    window.Content(rootPanel);
}

void Importer::log(const std::string& message)
{
    window.DispatcherQueue().TryEnqueue([this, message]() {
        hstring currentText = log_output.Text();
        if (!currentText.empty()) {
            log_output.Text(currentText + L"\n" + to_hstring(message));
        } else {
            log_output.Text(to_hstring(message));
        }

        log_scroll.UpdateLayout();
        log_scroll.ChangeView(nullptr, log_scroll.ScrollableHeight(), nullptr);

        if (log_file && log_file->is_open()) {
            *log_file << message << "\n";
            log_file->flush();
        }
    });
}

void Importer::validate_cs2()
{
    ShellExecuteA(NULL, "open", "steam://validate/730", NULL, NULL, SW_SHOWNORMAL);
}

void Importer::validate_s1()
{
    if (s1_game_type == "css") {
        ShellExecuteA(NULL, "open", "steam://validate/240", NULL, NULL, SW_SHOWNORMAL);
    } else if (s1_game_type == "csgo") {
        ShellExecuteA(NULL, "open", "steam://validate/4465480", NULL, NULL, SW_SHOWNORMAL);
    }
}

void Importer::set_stylesheets()
{
    cs2_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
    s1_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
    map_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::Red()));
}

void Importer::set_tooltips()
{
    ToolTipService::SetToolTip(cs2_button, box_value(to_hstring("Use \"Counter-Strike Global Offensive\" folder or any folder inside it.")));
    ToolTipService::SetToolTip(s1_button, box_value(to_hstring("Use \"csgo legacy\" folder or any folder inside it.")));
    ToolTipService::SetToolTip(vmf_button, box_value(to_hstring("Does not need to be in a \"maps\" folder, one will be created then deleted afterwards if necessary.")));
    ToolTipService::SetToolTip(usebsp_checkbox, box_value(to_hstring("This runs the map through a special vbsp process to generate clean map geometry from brushes...")));
    ToolTipService::SetToolTip(usebsp_nomergeinstances_checkbox, box_value(to_hstring("Use this instead of -usebsp if you wish to both generate clean geo and also preserve func_instances...")));
    ToolTipService::SetToolTip(skipdeps_checkbox, box_value(to_hstring("Optional: skips importing all dependencies/content and only generates the vmap file(s)...")));
}

static std::string SelectFolder(HWND hwndOwner, const std::string& defaultPath) {
    IFileOpenDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        pfd->GetOptions(&dwOptions);
        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

        hr = pfd->Show(hwndOwner);
        if (SUCCEEDED(hr)) {
            IShellItem* psi;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    std::string result = my_to_string(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                    psi->Release();
                    pfd->Release();
                    return result;
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    return "";
}

static std::string SelectFile(HWND hwndOwner, const std::string& defaultPath, const std::vector<std::pair<std::wstring, std::wstring>>& filters) {
    IFileOpenDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        std::vector<COMDLG_FILTERSPEC> spec;
        for (const auto& f : filters) {
            spec.push_back({f.first.c_str(), f.second.c_str()});
        }
        if (!spec.empty()) {
            pfd->SetFileTypes((UINT)spec.size(), spec.data());
        }

        hr = pfd->Show(hwndOwner);
        if (SUCCEEDED(hr)) {
            IShellItem* psi;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    std::string result = my_to_string(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                    psi->Release();
                    pfd->Release();
                    return result;
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    return "";
}

HWND GetWindowHandle(winrt::Microsoft::UI::Xaml::Window window) {
    HWND hwnd;
    window.as<IWindowNative>()->get_WindowHandle(&hwnd);
    return hwnd;
}

void Importer::select_cs2_folder()
{
    std::string path = SelectFolder(GetWindowHandle(window), vmf_default_path);
    if (path.empty()) return;

    fs::path gameinfo_path = fs::path(path) / "game" / "csgo" / "gameinfo.gi";
    bool valid = false;

    if (fs::exists(gameinfo_path)) {
        std::ifstream file(gameinfo_path);
        if (file.is_open()) {
            std::string line;
            std::regex regex("^\\s*game\\s+\"Counter-Strike 2\"\\s*$");
            while (std::getline(file, line)) {
                if (std::regex_match(line, regex)) {
                    valid = true;
                    break;
                }
            }
        }
    }

    if (!valid) {
        ShowError("Invalid CS2 Folder", "The selected folder is not a valid CS2 installation.\nPlease make sure to select a folder where game/csgo/gameinfo.gi contains 'game \"Counter-Strike 2\"'.");
        return;
    }

    set_cs2_folder(path);
}

void Importer::set_cs2_folder(const std::string& path)
{
    if (!path.empty() && path != "None") {
        cs2_basefolder = path;
        cs2_label.Text(L"Selected");
        cs2_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::LimeGreen()));
    }
}

void Importer::select_s1_folder()
{
    std::string path = SelectFolder(GetWindowHandle(window), vmf_default_path);
    if (path.empty()) return;

    std::string selected_game = my_to_string(unbox_value<hstring>(s1_game_combo.SelectedItem()));
    bool valid = false;

    if (selected_game == "CSGO") {
        fs::path gameinfo_path_csgo = fs::path(path) / "csgo" / "gameinfo.txt";
        if (fs::exists(gameinfo_path_csgo)) {
            std::ifstream file(gameinfo_path_csgo);
            std::string line;
            std::regex regex("^\\s*game\\s+\"Counter-Strike: Global Offensive\"\\s*$");
            while (std::getline(file, line)) {
                if (std::regex_match(line, regex)) {
                    valid = true;
                    s1_game_type = "csgo";
                    break;
                }
            }
        }
    } else if (selected_game == "CSS") {
        fs::path gameinfo_path_css = fs::path(path) / "cstrike" / "gameinfo.txt";
        if (fs::exists(gameinfo_path_css)) {
            std::ifstream file(gameinfo_path_css);
            std::string line;
            std::regex regex("^\\s*game\\s+\"Counter-Strike Source\"\\s*$");
            while (std::getline(file, line)) {
                if (std::regex_match(line, regex)) {
                    valid = true;
                    s1_game_type = "css";
                    break;
                }
            }
        }
    }

    if (!valid) {
        if (selected_game == "CSGO") {
            ShowError("Invalid Source 1 Folder", "The selected folder is not a valid CS:GO legacy installation.\nPlease make sure to select a folder where csgo/gameinfo.txt contains 'game \"Counter-Strike: Global Offensive\"'.");
        } else {
            ShowError("Invalid Source 1 Folder", "The selected folder is not a valid Counter-Strike Source installation.\nPlease make sure to select a folder where cstrike/gameinfo.txt contains 'game \"Counter-Strike Source\"'.");
        }
        return;
    }

    set_s1_folder(path);
}

void Importer::set_s1_folder(const std::string& path)
{
    if (!path.empty() && path != "None") {
        s1game_basefolder = path;
        s1_label.Text(L"Selected");
        s1_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::LimeGreen()));
    }
}

void Importer::select_vmf()
{
    std::string path = SelectFile(GetWindowHandle(window), vmf_default_path, {{L"VMF files (*.vmf)", L"*.vmf"}});
    if (path.empty()) return;

    bsp_file.clear();
    fs::path fileInfo(path);
    map_name = fileInfo.stem().string();
    content_folder = fileInfo.parent_path().string();

    fs::path target_maps_dir = fs::path(app_dir) / "maps" / map_name / "maps";
    fs::create_directories(target_maps_dir);

    fs::path target_vmf_path = target_maps_dir / fileInfo.filename();

    if (fileInfo != target_vmf_path) {
        if (fs::exists(target_vmf_path)) {
            fs::remove(target_vmf_path);
        }
        fs::copy_file(fileInfo, target_vmf_path);
    }

    content_folder_to_save = content_folder;
    content_folder = (fs::path(app_dir) / "maps" / map_name).string();
    log("VMF set up at: " + target_vmf_path.string());

    map_label.Text(L"Selected");
    map_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::LimeGreen()));
}

void Importer::select_bsp()
{
    std::string path = SelectFile(GetWindowHandle(window), vmf_default_path, {{L"BSP files (*.bsp)", L"*.bsp"}});
    if (path.empty()) return;

    bsp_file = path;
    fs::path fileInfo(path);
    map_name = fileInfo.stem().string();
    content_folder_to_save = fileInfo.parent_path().string();

    map_label.Text(L"Selected");
    map_label.Foreground(SolidColorBrush(Microsoft::UI::Colors::LimeGreen()));
}

void Importer::on_usebsp_toggled(bool checked)
{
    usebsp_nomergeinstances_checkbox.IsEnabled(checked);
    if (!checked) {
        usebsp_nomergeinstances_checkbox.IsChecked(false);
    }
}

void Importer::on_usebsp_nomergeinstances_toggled(bool checked)
{
    if (checked) {
        usebsp_checkbox.IsChecked(true);
    }
}

void Importer::get_addon_name()
{
    addon_name = my_to_string(addon_edit.Text());
}

void Importer::get_launch_options()
{
    std::vector<std::string> options;
    if (get_checked(usebsp_checkbox.IsChecked())) {
        if (get_checked(usebsp_nomergeinstances_checkbox.IsChecked())) {
            options.push_back("-usebsp_nomergeinstances");
        } else {
            options.push_back("-usebsp");
        }
    }
    if (get_checked(skipdeps_checkbox.IsChecked())) options.push_back("-skipdeps");

    launch_options.clear();
    for(size_t i=0; i<options.size(); ++i) {
        launch_options += options[i] + (i < options.size()-1 ? " " : "");
    }
}

void Importer::save_to_cfg()
{
    std::string usebsp_state = get_checked(usebsp_checkbox.IsChecked()) ? "True" : "False";
    std::string nomerge_state = get_checked(usebsp_nomergeinstances_checkbox.IsChecked()) ? "True" : "False";
    std::string skipdeps_state = get_checked(skipdeps_checkbox.IsChecked()) ? "True" : "False";

    std::ostringstream out;
    out << usebsp_state << "\n"
        << nomerge_state << "\n"
        << skipdeps_state << "\n"
        << cs2_basefolder << "\n"
        << s1game_basefolder << "\n"
        << content_folder_to_save << "\n"
        << s1_game_type;

    std::ofstream file("cs2importer.cfg");
    if (file.is_open()) {
        file << out.str();
        file.close();
    }
}

void Importer::load_from_cfg()
{
    if (!fs::exists("cs2importer.cfg")) {
        std::ofstream file("cs2importer.cfg");
        file.close();
        return;
    }

    std::ifstream file("cs2importer.cfg");
    if (file.is_open()) {
        std::vector<std::string> temp;
        std::string line;
        while (std::getline(file, line)) {
            temp.push_back(line);
        }
        file.close();

        if (temp.empty()) return;

        if (temp[0] == "True" || temp[0] == "False") {
            if (temp.size() >= 6) {
                usebsp_checkbox.IsChecked(temp[0] == "True");
                usebsp_nomergeinstances_checkbox.IsChecked(temp[1] == "True");
                usebsp_nomergeinstances_checkbox.IsEnabled(temp[0] == "True");
                skipdeps_checkbox.IsChecked(temp[2] == "True");
                set_cs2_folder(temp[3]);

                if (temp.size() >= 7) {
                    s1_game_type = temp[6];
                } else {
                    s1_game_type = "csgo"; // Assume CSGO for old configs
                }

                if (s1_game_type == "css") {
                    s1_game_combo.SelectedIndex(1);
                } else {
                    s1_game_combo.SelectedIndex(0);
                }

                set_s1_folder(temp[4]);
                vmf_default_path = temp[5];
            }
        } else {
            if (temp.size() == 3) {
                set_cs2_folder(temp[1]);
                vmf_default_path = temp[2];
            } else if (temp.size() >= 4) {
                set_cs2_folder(temp[1]);
                set_s1_folder(temp[2]);
                vmf_default_path = temp[3];
            }
        }
    }
}

void Importer::go()
{
    log_output.Text(L"");

    if (cs2_basefolder.empty()) {
        ShowError("Validation Error", "CS2 folder not selected.");
        return;
    }
    if (s1game_basefolder.empty()) {
        ShowError("Validation Error", "CSGO folder not selected.");
        return;
    }
    if (bsp_file.empty() && content_folder.empty()) {
        ShowError("Validation Error", "Please select a VMF or BSP file.");
        return;
    }

    try {
        get_addon_name();
        std::string temp_addon = addon_name;
        // trim whitespace manually
        temp_addon.erase(temp_addon.begin(), std::find_if(temp_addon.begin(), temp_addon.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        temp_addon.erase(std::find_if(temp_addon.rbegin(), temp_addon.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), temp_addon.end());

        if (temp_addon.empty()) {
            addon_name = map_name;
        }

        save_to_cfg();

        fs::path log_dir_path = fs::path(app_dir) / "log";
        fs::create_directories(log_dir_path);

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm bt;
#if defined(_WIN32)
        localtime_s(&bt, &now_time);
#else
        localtime_r(&now_time, &bt);
#endif
        std::ostringstream time_oss;
        time_oss << std::put_time(&bt, "%Y-%m-%d_%H-%M-%S");

        std::string log_filename = time_oss.str() + "_" + addon_name + ".log";
        fs::path log_file_path = log_dir_path / log_filename;

        if (log_file) {
            if (log_file->is_open()) {
                log_file->close();
            }
            delete log_file;
            log_file = nullptr;
        }

        log_file = new std::ofstream(log_file_path, std::ios_base::app);

        AppCore::cancel_import = false;
        AppCore::move_vpk_signatures(cs2_basefolder, vpk_signatures_moved);

        go_button.IsEnabled(false);
        log("Starting AppCore thread...");

        AppCore::Options opts;
        std::string cs2_base_clean = cs2_basefolder;
        std::replace(cs2_base_clean.begin(), cs2_base_clean.end(), '/', '\\');
        opts.cs2_basefolder = cs2_base_clean;
        opts.s1game_basefolder = s1game_basefolder;
        opts.s1_game_type = s1_game_type;
        opts.content_folder = content_folder;
        opts.map_name = map_name;
        opts.bsp_file = bsp_file;
        opts.app_dir = app_dir;
        opts.addon_name = addon_name;
        opts.usebsp = get_checked(usebsp_checkbox.IsChecked()) && !get_checked(usebsp_nomergeinstances_checkbox.IsChecked());
        opts.usebsp_nomergeinstances = get_checked(usebsp_checkbox.IsChecked()) && get_checked(usebsp_nomergeinstances_checkbox.IsChecked());
        opts.skipdeps = get_checked(skipdeps_checkbox.IsChecked());

        opts.logger = [this](const std::string& msg) {
            log(msg);
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
                std::replace(s1gamedir.begin(), s1gamedir.end(), '/', '\\');
                mapOpts.s1gamedir = s1gamedir;

                mapOpts.s1gamename = opts.s1_game_type == "css" ? "css" : "csgo";

                std::string contentdir = opts.content_folder;
                std::replace(contentdir.begin(), contentdir.end(), '/', '\\');
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

            window.DispatcherQueue().TryEnqueue([this, success]() {
                if (vpk_signatures_moved && !cs2_basefolder.empty()) {
                    AppCore::restore_vpk_signatures(cs2_basefolder);
                    vpk_signatures_moved = false;
                }
                go_button.IsEnabled(true);
                if (success) {
                    log("MapImporter thread finished successfully.");
                } else {
                    log("MapImporter thread finished with errors.");
                }

                if (log_file) {
                    if (log_file->is_open()) {
                        log_file->close();
                    }
                    delete log_file;
                    log_file = nullptr;
                }
            });

        }).detach();

    } catch (const std::exception& e) {
        log(std::string("Error: ") + e.what());
        ShowError("Error", e.what());
    }
}
