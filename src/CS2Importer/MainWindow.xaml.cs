using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.IO;
using System.Threading.Tasks;
using Windows.System;
using Windows.Storage.Pickers;
using System.Text.RegularExpressions;
using System.Threading;

namespace CS2Importer
{
    public sealed partial class MainWindow : Window
    {
        private string app_dir;
        private bool java_installed;
        private string vmf_default_path = "C:\\";
        private string cs2_basefolder = "";
        private string s1game_basefolder = "";
        private string s1_game_type = "csgo"; // "csgo" or "css"
        private string content_folder = "";
        private string content_folder_to_save = "C:\\";
        private string addon_name = "";
        private string map_name = "";
        private bool vpk_signatures_moved = false;
        private string bsp_file = "";

        private StreamWriter? log_stream;

        public MainWindow()
        {
            this.InitializeComponent();

            app_dir = AppDomain.CurrentDomain.BaseDirectory;

            s1GameCombo.SelectedIndex = 0; // Default to CSGO

            Log("Initializing CS2 Importer...");

            java_installed = AppCore.CheckJava();

            if (!java_installed)
            {
                selectBspButton.IsEnabled = false;
                ToolTipService.SetToolTip(selectBspButton, "Java is missing. BSP decompilation is disabled.");
                Log("Warning: Java is missing. BSP decompilation disabled.");
            }

            LoadFromCfg();

            this.Closed += MainWindow_Closed;

            Log("Initializing CS2 Importer... Finished");
        }

        private void Log(string message)
        {
            DispatcherQueue.TryEnqueue(() =>
            {
                logOutputTextBlock.Text += message + "\n";
                // Scroll to bottom functionality is implicit or can be added via ScrollViewer interactions
            });

            if (log_stream != null)
            {
                try
                {
                    log_stream.WriteLine(message);
                    log_stream.Flush();
                }
                catch { }
            }
        }

        private async void SelectCs2Folder_Click(object sender, RoutedEventArgs e)
        {
            var folderPicker = new FolderPicker();
            folderPicker.SuggestedStartLocation = PickerLocationId.Desktop;
            folderPicker.FileTypeFilter.Add("*");

            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(folderPicker, hwnd);

            var folder = await folderPicker.PickSingleFolderAsync();
            if (folder != null)
            {
                string path = folder.Path;
                string gameinfo_path = Path.Combine(path, "game", "csgo", "gameinfo.gi");

                bool valid = false;
                if (File.Exists(gameinfo_path))
                {
                    string content = File.ReadAllText(gameinfo_path);
                    if (Regex.IsMatch(content, @"^\s*game\s+""Counter-Strike 2""\s*$", RegexOptions.Multiline))
                    {
                        valid = true;
                    }
                }

                if (!valid)
                {
                    await ShowDialogAsync("Invalid CS2 Folder", "The selected folder is not a valid CS2 installation.\nPlease make sure to select a folder where game/csgo/gameinfo.gi contains 'game \"Counter-Strike 2\"'.");
                }
                else
                {
                    SetCs2Folder(path);
                }
            }
        }

        private void SetCs2Folder(string path)
        {
            if (!string.IsNullOrEmpty(path) && path != "None")
            {
                cs2_basefolder = path;
                cs2PathTextBox.Text = path;
            }
        }

        private async void SelectS1Folder_Click(object sender, RoutedEventArgs e)
        {
            var folderPicker = new FolderPicker();
            folderPicker.SuggestedStartLocation = PickerLocationId.Desktop;
            folderPicker.FileTypeFilter.Add("*");

            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(folderPicker, hwnd);

            var folder = await folderPicker.PickSingleFolderAsync();
            if (folder != null)
            {
                string path = folder.Path;
                string selected_game = s1GameCombo.SelectedItem?.ToString() ?? "CSGO";
                bool valid = false;

                if (selected_game == "CSGO")
                {
                    string gameinfo_path = Path.Combine(path, "csgo", "gameinfo.txt");
                    if (File.Exists(gameinfo_path))
                    {
                        string content = File.ReadAllText(gameinfo_path);
                        if (Regex.IsMatch(content, @"^\s*game\s+""Counter-Strike: Global Offensive""\s*$", RegexOptions.Multiline))
                        {
                            valid = true;
                            s1_game_type = "csgo";
                        }
                    }
                }
                else if (selected_game == "CSS")
                {
                    string gameinfo_path = Path.Combine(path, "cstrike", "gameinfo.txt");
                    if (File.Exists(gameinfo_path))
                    {
                        string content = File.ReadAllText(gameinfo_path);
                        if (Regex.IsMatch(content, @"^\s*game\s+""Counter-Strike Source""\s*$", RegexOptions.Multiline))
                        {
                            valid = true;
                            s1_game_type = "css";
                        }
                    }
                }

                if (!valid)
                {
                    string msg = selected_game == "CSGO" ?
                        "The selected folder is not a valid CS:GO legacy installation.\nPlease make sure to select a folder where csgo/gameinfo.txt contains 'game \"Counter-Strike: Global Offensive\"'." :
                        "The selected folder is not a valid Counter-Strike Source installation.\nPlease make sure to select a folder where cstrike/gameinfo.txt contains 'game \"Counter-Strike Source\"'.";
                    await ShowDialogAsync("Invalid Source 1 Folder", msg);
                }
                else
                {
                    SetS1Folder(path);
                }
            }
        }

        private void SetS1Folder(string path)
        {
            if (!string.IsNullOrEmpty(path) && path != "None")
            {
                s1game_basefolder = path;
                s1PathTextBox.Text = path;
            }
        }

        private async void SelectVmf_Click(object sender, RoutedEventArgs e)
        {
            var picker = new FileOpenPicker();
            picker.SuggestedStartLocation = PickerLocationId.Desktop;
            picker.FileTypeFilter.Add(".vmf");

            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);

            var file = await picker.PickSingleFileAsync();
            if (file != null)
            {
                bsp_file = "";
                string path = file.Path;
                map_name = Path.GetFileNameWithoutExtension(path);
                content_folder = Path.GetDirectoryName(path) ?? "";

                string target_maps_dir = Path.Combine(app_dir, "maps", map_name, "maps");
                Directory.CreateDirectory(target_maps_dir);

                string target_vmf_path = Path.Combine(target_maps_dir, Path.GetFileName(path));

                if (path != target_vmf_path)
                {
                    if (File.Exists(target_vmf_path)) File.Delete(target_vmf_path);
                    File.Copy(path, target_vmf_path);
                }

                content_folder_to_save = content_folder;
                content_folder = Path.Combine(app_dir, "maps", map_name);
                Log("VMF set up at: " + target_vmf_path);

                mapPathTextBox.Text = path;
            }
        }

        private async void SelectBsp_Click(object sender, RoutedEventArgs e)
        {
            var picker = new FileOpenPicker();
            picker.SuggestedStartLocation = PickerLocationId.Desktop;
            picker.FileTypeFilter.Add(".bsp");

            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);

            var file = await picker.PickSingleFileAsync();
            if (file != null)
            {
                string path = file.Path;
                bsp_file = path;
                map_name = Path.GetFileNameWithoutExtension(path);
                content_folder_to_save = Path.GetDirectoryName(path) ?? "";

                mapPathTextBox.Text = path;
            }
        }

        private async void ValidateCs2_Click(object sender, RoutedEventArgs e)
        {
            await Launcher.LaunchUriAsync(new Uri("steam://validate/730"));
        }

        private async void ValidateS1_Click(object sender, RoutedEventArgs e)
        {
            if (s1_game_type == "css")
            {
                await Launcher.LaunchUriAsync(new Uri("steam://validate/240"));
            }
            else if (s1_game_type == "csgo")
            {
                await Launcher.LaunchUriAsync(new Uri("steam://validate/4465480"));
            }
        }

        private void UseBspCheckBox_Changed(object sender, RoutedEventArgs e)
        {
            if (useBspCheckBox.IsChecked == true)
            {
                useBspNoMergeCheckBox.IsEnabled = true;
            }
            else
            {
                useBspNoMergeCheckBox.IsEnabled = false;
                useBspNoMergeCheckBox.IsChecked = false;
            }
        }

        private void UseBspNoMergeCheckBox_Changed(object sender, RoutedEventArgs e)
        {
            if (useBspNoMergeCheckBox.IsChecked == true)
            {
                useBspCheckBox.IsChecked = true;
            }
        }

        private void S1GameCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            s1game_basefolder = "";
            s1PathTextBox.Text = "";
        }

        private void AddonNameTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            addon_name = addonNameTextBox.Text;
        }

        private async void GoButton_Click(object sender, RoutedEventArgs e)
        {
            logOutputTextBlock.Text = "";

            if (string.IsNullOrEmpty(cs2_basefolder))
            {
                await ShowDialogAsync("Validation Error", "CS2 folder not selected.");
                return;
            }
            if (string.IsNullOrEmpty(s1game_basefolder))
            {
                await ShowDialogAsync("Validation Error", "CSGO/CSS folder not selected.");
                return;
            }
            if (string.IsNullOrEmpty(bsp_file) && string.IsNullOrEmpty(content_folder))
            {
                await ShowDialogAsync("Validation Error", "Please select a VMF or BSP file.");
                return;
            }

            try
            {
                addon_name = addonNameTextBox.Text;
                if (string.IsNullOrWhiteSpace(addon_name))
                {
                    addon_name = map_name;
                }

                SaveToCfg();

                string log_dir_path = Path.Combine(app_dir, "log");
                Directory.CreateDirectory(log_dir_path);
                string log_filename = $"{DateTime.Now:yyyy-MM-dd_HH-mm-ss}_{addon_name}.log";
                string log_file_path = Path.Combine(log_dir_path, log_filename);

                if (log_stream != null)
                {
                    log_stream.Close();
                    log_stream = null;
                }

                log_stream = new StreamWriter(new FileStream(log_file_path, FileMode.Append, FileAccess.Write, FileShare.ReadWrite));

                AppCore.MoveVpkSignatures(cs2_basefolder, out vpk_signatures_moved);

                goButton.IsEnabled = false;
                Log("Starting AppCore thread...");

                var opts = new AppCore.Options
                {
                    cs2_basefolder = cs2_basefolder.Replace("/", "\\"),
                    s1game_basefolder = s1game_basefolder,
                    s1_game_type = s1_game_type,
                    content_folder = content_folder,
                    map_name = map_name,
                    bsp_file = bsp_file,
                    app_dir = app_dir,
                    addon_name = addon_name,
                    usebsp = (useBspCheckBox.IsChecked == true) && (useBspNoMergeCheckBox.IsChecked != true),
                    usebsp_nomergeinstances = (useBspCheckBox.IsChecked == true) && (useBspNoMergeCheckBox.IsChecked == true),
                    skipdeps = skipDepsCheckBox.IsChecked == true,
                    logger = Log
                };

                _ = Task.Run(() =>
                {
                    bool success = true;
                    try
                    {
                        if (!string.IsNullOrEmpty(opts.bsp_file))
                        {
                            if (!AppCore.CheckJava())
                            {
                                throw new Exception("Java is not installed. Cannot decompile BSP file.");
                            }
                            AppCore.ProcessBsp(opts);
                        }

                        var mapOpts = new MapImporter.Options();
                        string s1_subfolder = opts.s1_game_type == "css" ? "cstrike" : "csgo";

                        mapOpts.s1gamedir = Path.Combine(opts.s1game_basefolder, s1_subfolder).Replace("/", "\\");
                        mapOpts.s1gamename = opts.s1_game_type == "css" ? "css" : "csgo";
                        mapOpts.s1contentdir = opts.content_folder.Replace("/", "\\");
                        mapOpts.s2addonname = opts.addon_name;
                        mapOpts.s2contentdir = Path.Combine(opts.cs2_basefolder, "content", "csgo_addons", opts.addon_name);
                        mapOpts.mapname = opts.map_name;
                        mapOpts.usebsp = opts.usebsp;
                        mapOpts.usebsp_nomergeinstances = opts.usebsp_nomergeinstances;
                        mapOpts.skipdeps = opts.skipdeps;
                        mapOpts.cs2_basefolder = opts.cs2_basefolder;

                        var importer = new MapImporter(mapOpts, opts.logger);
                        success = importer.Run();
                    }
                    catch (Exception ex)
                    {
                        opts.logger?.Invoke($"Error: {ex.Message}");
                        success = false;
                    }

                    DispatcherQueue.TryEnqueue(() =>
                    {
                        goButton.IsEnabled = true;
                        if (success)
                        {
                            Log("MapImporter thread finished successfully.");
                        }
                        else
                        {
                            Log("MapImporter thread finished with errors.");
                        }

                        if (log_stream != null)
                        {
                            log_stream.Close();
                            log_stream = null;
                        }
                    });
                });

            }
            catch (Exception ex)
            {
                Log($"Error: {ex.Message}");
                _ = ShowDialogAsync("Error", ex.Message);
            }
        }

        private void SaveToCfg()
        {
            string usebsp_state = useBspCheckBox.IsChecked == true ? "True" : "False";
            string nomerge_state = useBspNoMergeCheckBox.IsChecked == true ? "True" : "False";
            string skipdeps_state = skipDepsCheckBox.IsChecked == true ? "True" : "False";

            string temp = $"{usebsp_state}\n{nomerge_state}\n{skipdeps_state}\n{cs2_basefolder}\n{s1game_basefolder}\n{content_folder_to_save}\n{s1_game_type}";

            string cfgPath = Path.Combine(app_dir, "cs2importer.cfg");
            File.WriteAllText(cfgPath, temp);
        }

        private void LoadFromCfg()
        {
            string cfgPath = Path.Combine(app_dir, "cs2importer.cfg");
            if (!File.Exists(cfgPath))
            {
                File.Create(cfgPath).Close();
                return;
            }

            var temp = File.ReadAllLines(cfgPath);
            if (temp.Length == 0) return;

            if (temp[0] == "True" || temp[0] == "False")
            {
                if (temp.Length >= 6)
                {
                    useBspCheckBox.IsChecked = temp[0] == "True";
                    useBspNoMergeCheckBox.IsChecked = temp[1] == "True";
                    useBspNoMergeCheckBox.IsEnabled = temp[0] == "True";
                    skipDepsCheckBox.IsChecked = temp[2] == "True";
                    SetCs2Folder(temp[3]);

                    if (temp.Length >= 7)
                    {
                        s1_game_type = temp[6];
                    }
                    else
                    {
                        s1_game_type = "csgo";
                    }

                    if (s1_game_type == "css")
                    {
                        s1GameCombo.SelectedItem = "CSS";
                    }
                    else
                    {
                        s1GameCombo.SelectedItem = "CSGO";
                    }

                    SetS1Folder(temp[4]);
                    vmf_default_path = temp[5];
                }
            }
            else
            {
                if (temp.Length == 3)
                {
                    SetCs2Folder(temp[1]);
                    vmf_default_path = temp[2];
                }
                else if (temp.Length >= 4)
                {
                    SetCs2Folder(temp[1]);
                    SetS1Folder(temp[2]);
                    vmf_default_path = temp[3];
                }
            }
        }

        private void MainWindow_Closed(object sender, WindowEventArgs args)
        {
            if (vpk_signatures_moved && !string.IsNullOrEmpty(cs2_basefolder))
            {
                AppCore.RestoreVpkSignatures(cs2_basefolder);
            }
        }

        private async Task ShowDialogAsync(string title, string content)
        {
            ContentDialog dialog = new ContentDialog
            {
                Title = title,
                Content = content,
                CloseButtonText = "OK",
                XamlRoot = this.Content.XamlRoot
            };
            await dialog.ShowAsync();
        }
    }
}
