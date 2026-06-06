#pragma once

#include <string>
#include <fstream>
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

class Importer
{
public:
    Importer();
    ~Importer();

    winrt::Microsoft::UI::Xaml::Window GetWindow() const { return window; }

private:
    void select_cs2_folder();
    void select_s1_folder();
    void select_vmf();
    void select_bsp();
    void validate_cs2();
    void validate_s1();
    void on_usebsp_toggled(bool checked);
    void on_usebsp_nomergeinstances_toggled(bool checked);
    void get_addon_name();
    void get_launch_options();
    void go();

public:
    void log(const std::string& message);

private:
    winrt::Microsoft::UI::Xaml::Window window{ nullptr };

    // UI elements
    winrt::Microsoft::UI::Xaml::Controls::Button cs2_button{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock cs2_label{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Button validate_cs2_button{ nullptr };

    winrt::Microsoft::UI::Xaml::Controls::ComboBox s1_game_combo{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Button s1_button{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock s1_label{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Button validate_s1_button{ nullptr };

    winrt::Microsoft::UI::Xaml::Controls::Button vmf_button{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Button bsp_button{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock map_label{ nullptr };

    winrt::Microsoft::UI::Xaml::Controls::TextBox addon_edit{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::CheckBox usebsp_checkbox{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::CheckBox usebsp_nomergeinstances_checkbox{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::CheckBox skipdeps_checkbox{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::Button go_button{ nullptr };

    winrt::Microsoft::UI::Xaml::Controls::ScrollViewer log_scroll{ nullptr };
    winrt::Microsoft::UI::Xaml::Controls::TextBlock log_output{ nullptr };

    std::string app_dir;
    bool java_installed;
    std::string vmf_default_path;
    std::string cs2_basefolder;
    std::string s1game_basefolder;
    std::string s1_game_type; // "csgo" or "css"
    std::string content_folder;
    std::string content_folder_to_save;
    std::string addon_name;
    std::string map_name;
    bool vpk_signatures_moved;
    std::string bsp_file;
    std::string launch_options;

    std::ofstream* log_file;

    void set_cs2_folder(const std::string& path);
    void set_s1_folder(const std::string& path);

    void set_tooltips();
    void set_stylesheets();
    void load_from_cfg();
    void save_to_cfg();

    void create_ui();
};
