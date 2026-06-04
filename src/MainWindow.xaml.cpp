#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "appcore.h"
#include "mapimporter.h"

#include <shobjidl.h>
#include <shellapi.h>
#include <fstream>
#include <regex>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Storage::Pickers;

namespace winrt::cs2importer::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        this->Closed({ this, &MainWindow::Closed });
        m_dispatcherQueue = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        LoadConfig();
    }

    void MainWindow::Log(std::string const& message)
    {
        if (m_dispatcherQueue)
        {
            m_dispatcherQueue.TryEnqueue([this, msg = message]()
            {
                auto currentText = LogText().Text();
                LogText().Text(currentText + winrt::to_hstring(msg) + L"\n");

                // Scroll to bottom manually by forcing the ScrollViewer to update
                LogScroll().UpdateLayout();
                LogScroll().ChangeView(nullptr, LogScroll().ScrollableHeight(), nullptr);
            });
        }
    }

    winrt::fire_and_forget SelectFolderAsync(HWND hwnd, std::function<void(std::wstring)> callback)
    {
        FolderPicker folderPicker;
        auto initializeWithWindow{ folderPicker.as<::IInitializeWithWindow>() };
        initializeWithWindow->Initialize(hwnd);
        folderPicker.FileTypeFilter().Append(L"*");
        auto folder = co_await folderPicker.PickSingleFolderAsync();
        if (folder)
        {
            callback(folder.Path().c_str());
        }
    }

    winrt::fire_and_forget SelectFileAsync(HWND hwnd, std::vector<std::wstring> const& extensions, std::function<void(std::wstring)> callback)
    {
        FileOpenPicker filePicker;
        auto initializeWithWindow{ filePicker.as<::IInitializeWithWindow>() };
        initializeWithWindow->Initialize(hwnd);
        for (auto const& ext : extensions)
            filePicker.FileTypeFilter().Append(ext);

        auto file = co_await filePicker.PickSingleFileAsync();
        if (file)
        {
            callback(file.Path().c_str());
        }
    }

    void MainWindow::OnS1FolderClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto hwnd = winrt::Microsoft::UI::GetWindowFromWindowId(this->AppWindow().Id());
        SelectFolderAsync(reinterpret_cast<HWND>(hwnd), [this](std::wstring path) {
            s1_folder = winrt::to_string(path);
            S1FolderBtn().Content(winrt::box_value(winrt::to_hstring(s1_folder)));
            SaveConfig();
        });
    }

    void MainWindow::OnCS2FolderClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto hwnd = winrt::Microsoft::UI::GetWindowFromWindowId(this->AppWindow().Id());
        SelectFolderAsync(reinterpret_cast<HWND>(hwnd), [this](std::wstring path) {
            cs2_folder = winrt::to_string(path);
            CS2FolderBtn().Content(winrt::box_value(winrt::to_hstring(cs2_folder)));
            SaveConfig();
        });
    }

    void MainWindow::OnS1ValidateClick(IInspectable const&, RoutedEventArgs const&)
    {
        std::wstring url = L"steam://validate/240";
        if (S1GameCombo().SelectedIndex() == 0) url = L"steam://validate/4465480";
        ShellExecute(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void MainWindow::OnCS2ValidateClick(IInspectable const&, RoutedEventArgs const&)
    {
        ShellExecute(nullptr, L"open", L"steam://validate/730", nullptr, nullptr, SW_SHOWNORMAL);
    }

    void MainWindow::OnVmfClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto hwnd = winrt::Microsoft::UI::GetWindowFromWindowId(this->AppWindow().Id());
        SelectFileAsync(reinterpret_cast<HWND>(hwnd), {L".vmf"}, [this](std::wstring path) {
            vmf_file = winrt::to_string(path);
            std::filesystem::path p(vmf_file);
            map_name = p.stem().string();
            content_folder = p.parent_path().string();

            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            app_dir = std::filesystem::path(buffer).parent_path().string();

            std::filesystem::path target_maps_dir = std::filesystem::path(app_dir) / "maps" / map_name / "maps";
            std::filesystem::create_directories(target_maps_dir);

            std::filesystem::path target_vmf_path = target_maps_dir / p.filename();

            if (p != target_vmf_path) {
                if (std::filesystem::exists(target_vmf_path)) {
                    std::filesystem::remove(target_vmf_path);
                }
                std::filesystem::copy_file(p, target_vmf_path);
            }

            content_folder = (std::filesystem::path(app_dir) / "maps" / map_name).string();
            Log("VMF set up at: " + target_vmf_path.string());

            VmfBtn().Content(winrt::box_value(winrt::to_hstring(map_name + ".vmf")));
            bsp_file = "";
        });
    }

    void MainWindow::OnBspClick(IInspectable const&, RoutedEventArgs const&)
    {
        auto hwnd = winrt::Microsoft::UI::GetWindowFromWindowId(this->AppWindow().Id());
        SelectFileAsync(reinterpret_cast<HWND>(hwnd), {L".bsp"}, [this](std::wstring path) {
            bsp_file = winrt::to_string(path);
            std::filesystem::path p(bsp_file);
            map_name = p.stem().string();
            BspBtn().Content(winrt::box_value(winrt::to_hstring(map_name + ".bsp")));
            vmf_file = "";
        });
    }

    void MainWindow::OnUseBspChecked(IInspectable const&, RoutedEventArgs const&)
    {
        bool isChecked = (UseBspCheck().IsChecked() && UseBspCheck().IsChecked().Value()) ? true : false;
        KeepInstancesCheck().IsEnabled(isChecked);
        if (!isChecked) {
            KeepInstancesCheck().IsChecked(false);
        }
    }

    void MainWindow::OnHideLogClick(IInspectable const&, RoutedEventArgs const&)
    {
        if (LogScroll().Visibility() == Visibility::Visible) {
            LogScroll().Visibility(Visibility::Collapsed);
            HideLogBtn().Content(winrt::box_value(L"SHOW"));
        } else {
            LogScroll().Visibility(Visibility::Visible);
            HideLogBtn().Content(winrt::box_value(L"HIDE"));
        }
    }

    void MainWindow::OnStartClick(IInspectable const&, RoutedEventArgs const&)
    {
        LogText().Text(L"");
        SaveConfig();

        AppCore::Options opts;
        opts.cs2_basefolder = cs2_folder;
        opts.s1game_basefolder = s1_folder;
        opts.s1_game_type = (S1GameCombo().SelectedIndex() == 0) ? "csgo" : "css";
        opts.content_folder = content_folder;
        opts.map_name = map_name;
        opts.bsp_file = bsp_file;

        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        opts.app_dir = std::filesystem::path(buffer).parent_path().string();

        opts.addon_name = winrt::to_string(AddonNameBox().Text());
        if (opts.addon_name.empty()) opts.addon_name = opts.map_name;

        opts.usebsp = (UseBspCheck().IsChecked() && UseBspCheck().IsChecked().Value()) && !(KeepInstancesCheck().IsChecked() && KeepInstancesCheck().IsChecked().Value());
        opts.usebsp_nomergeinstances = (UseBspCheck().IsChecked() && UseBspCheck().IsChecked().Value()) && (KeepInstancesCheck().IsChecked() && KeepInstancesCheck().IsChecked().Value());
        opts.skipdeps = (SkipDepsCheck().IsChecked() && SkipDepsCheck().IsChecked().Value());

        std::string log_dir_path = (std::filesystem::path(opts.app_dir) / "log").string();
        std::filesystem::create_directories(log_dir_path);

        SYSTEMTIME st;
        GetLocalTime(&st);
        char log_filename[256];
        sprintf_s(log_filename, "%04d-%02d-%02d_%02d-%02d-%02d_%s.log",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, opts.addon_name.c_str());

        std::string log_file_path = (std::filesystem::path(log_dir_path) / log_filename).string();
        auto log_file = std::make_shared<std::ofstream>(log_file_path, std::ios::app);

        opts.logger = [this, log_file](std::string const& msg) {
            Log(msg);
            if (log_file && log_file->is_open()) {
                *log_file << msg << "\n";
                log_file->flush();
            }
        };

        AppCore::move_vpk_signatures(opts.cs2_basefolder, vpk_signatures_moved);

        StartBtn().IsEnabled(false);

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
                for (auto& c : s1gamedir) if (c == '/') c = '\\';
                mapOpts.s1gamedir = s1gamedir;

                mapOpts.s1gamename = opts.s1_game_type == "css" ? "css" : "csgo";

                std::string contentdir = opts.content_folder;
                for (auto& c : contentdir) if (c == '/') c = '\\';
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

            if (m_dispatcherQueue) {
                m_dispatcherQueue.TryEnqueue([this, success]() {
                    StartBtn().IsEnabled(true);
                    if (success) {
                        Log("Process completed successfully.");
                    } else {
                        Log("Process completed with errors.");
                    }
                });
            }
        }).detach();
    }

    void MainWindow::SaveConfig()
    {
        std::ofstream file("cs2importer.cfg");
        if (file.is_open()) {
            file << ((UseBspCheck().IsChecked() && UseBspCheck().IsChecked().Value()) ? "True" : "False") << "\n";
            file << ((KeepInstancesCheck().IsChecked() && KeepInstancesCheck().IsChecked().Value()) ? "True" : "False") << "\n";
            file << ((SkipDepsCheck().IsChecked() && SkipDepsCheck().IsChecked().Value()) ? "True" : "False") << "\n";
            file << cs2_folder << "\n";
            file << s1_folder << "\n";
            file << content_folder << "\n";
            file << ((S1GameCombo().SelectedIndex() == 0) ? "csgo" : "css") << "\n";
        }
    }

    void MainWindow::LoadConfig()
    {
        std::ifstream file("cs2importer.cfg");
        std::string line;
        std::vector<std::string> temp;
        while (std::getline(file, line)) {
            temp.push_back(line);
        }

        if (temp.size() >= 7) {
            UseBspCheck().IsChecked(temp[0] == "True");
            KeepInstancesCheck().IsChecked(temp[1] == "True");
            SkipDepsCheck().IsChecked(temp[2] == "True");

            cs2_folder = temp[3];
            if (!cs2_folder.empty()) CS2FolderBtn().Content(winrt::box_value(winrt::to_hstring(cs2_folder)));

            s1_folder = temp[4];
            if (!s1_folder.empty()) S1FolderBtn().Content(winrt::box_value(winrt::to_hstring(s1_folder)));

            content_folder = temp[5];

            S1GameCombo().SelectedIndex((temp[6] == "css") ? 1 : 0);
        }
    }

    void MainWindow::Closed(winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::WindowEventArgs const&)
    {
        if (vpk_signatures_moved && !cs2_folder.empty()) {
            AppCore::restore_vpk_signatures(cs2_folder);
        }
    }
}
