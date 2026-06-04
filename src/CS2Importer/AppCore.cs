using System;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Linq;

namespace CS2Importer
{
    public class AppCore
    {
        public delegate void LogCallback(string message);

        public class Options
        {
            public string cs2_basefolder = "";
            public string s1game_basefolder = "";
            public string s1_game_type = "";
            public string content_folder = "";
            public string map_name = "";
            public string bsp_file = "";
            public string app_dir = "";
            public string addon_name = "";
            public bool usebsp = false;
            public bool usebsp_nomergeinstances = false;
            public bool skipdeps = false;
            public LogCallback? logger = null;
        }

        public static bool CheckJava()
        {
            try
            {
                var processInfo = new ProcessStartInfo("java", "-version")
                {
                    RedirectStandardError = true,
                    RedirectStandardOutput = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                using var process = Process.Start(processInfo);
                if (process == null) return false;

                string output = process.StandardError.ReadToEnd() + process.StandardOutput.ReadToEnd();
                process.WaitForExit();

                return output.Contains("version");
            }
            catch
            {
                return false;
            }
        }

        public static void MoveVpkSignatures(string cs2Basefolder, out bool vpkSignaturesMoved)
        {
            vpkSignaturesMoved = false;
            if (string.IsNullOrEmpty(cs2Basefolder)) return;

            string binFolder = Path.Combine(cs2Basefolder, "game", "bin", "win64");
            string vpkPath = Path.Combine(binFolder, "vpk.signatures");
            string tempFolder = Path.Combine(binFolder, "temp");
            string tempVpkPath = Path.Combine(tempFolder, "vpk.signatures");

            if (File.Exists(vpkPath))
            {
                if (!Directory.Exists(tempFolder))
                {
                    Directory.CreateDirectory(tempFolder);
                }
                if (File.Exists(tempVpkPath))
                {
                    File.Delete(tempVpkPath);
                }
                File.Move(vpkPath, tempVpkPath);
                vpkSignaturesMoved = true;
            }
        }

        public static void RestoreVpkSignatures(string cs2Basefolder)
        {
            if (string.IsNullOrEmpty(cs2Basefolder)) return;

            string binFolder = Path.Combine(cs2Basefolder, "game", "bin", "win64");
            string vpkPath = Path.Combine(binFolder, "vpk.signatures");
            string tempVpkPath = Path.Combine(binFolder, "temp", "vpk.signatures");

            if (File.Exists(tempVpkPath))
            {
                if (File.Exists(vpkPath))
                {
                    File.Delete(vpkPath);
                }
                File.Move(tempVpkPath, vpkPath);
            }
        }

        public static int RunCommandSync(string cmd, LogCallback logger)
        {
            logger(cmd);

            try
            {
                // In C#, instead of passing the whole string to cmd.exe or CreateProcess,
                // we can just use cmd.exe /c
                var processInfo = new ProcessStartInfo("cmd.exe", "/c " + cmd)
                {
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                using var process = new Process { StartInfo = processInfo };

                process.OutputDataReceived += (sender, args) =>
                {
                    if (args.Data != null)
                        logger(args.Data);
                };
                process.ErrorDataReceived += (sender, args) =>
                {
                    if (args.Data != null)
                        logger(args.Data);
                };

                process.Start();
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();
                process.WaitForExit();

                return process.ExitCode;
            }
            catch (Exception ex)
            {
                logger($"Failed to run command: {ex.Message}");
                return -1;
            }
        }

        private static void FixVmfFromBsp(string vmfPath, LogCallback logger)
        {
            if (!File.Exists(vmfPath)) return;

            var lines = File.ReadAllLines(vmfPath).ToList();
            string mapversion = "2";
            var mapversionRegex = new Regex(@"^\s*""mapversion""\s+""([^""]+)""");
            bool mapversionFound = false;

            foreach (var line in lines)
            {
                var match = mapversionRegex.Match(line);
                if (match.Success)
                {
                    mapversion = match.Groups[1].Value;
                    mapversionFound = true;
                    break;
                }
            }

            if (!mapversionFound)
            {
                logger("No mapversion found in VMF. Aborting fix.");
                return;
            }

            int visgroupsStartIdx = -1;
            int visgroupsEndIdx = -1;
            bool hasVersioninfo = false;
            bool hasViewsettings = false;
            bool hasCordon = false;

            for (int i = 0; i < lines.Count; i++)
            {
                string trimmed = lines[i].Trim();
                if (trimmed == "visgroups" && visgroupsStartIdx == -1) visgroupsStartIdx = i;
                if (trimmed == "versioninfo") hasVersioninfo = true;
                if (trimmed == "viewsettings") hasViewsettings = true;
                if (trimmed == "cordon") hasCordon = true;
            }

            List<string> visgroupsLines = new List<string>();
            if (visgroupsStartIdx != -1)
            {
                int openBrackets = 0;
                bool foundFirstBracket = false;
                for (int i = visgroupsStartIdx; i < lines.Count; i++)
                {
                    openBrackets += lines[i].Count(c => c == '{');
                    openBrackets -= lines[i].Count(c => c == '}');
                    if (lines[i].Contains('{')) foundFirstBracket = true;

                    if (foundFirstBracket && openBrackets == 0)
                    {
                        visgroupsEndIdx = i;
                        break;
                    }
                }

                if (visgroupsEndIdx != -1)
                {
                    for (int i = visgroupsStartIdx; i <= visgroupsEndIdx; i++)
                    {
                        visgroupsLines.Add(lines[i]);
                    }
                    lines.RemoveRange(visgroupsStartIdx, visgroupsEndIdx - visgroupsStartIdx + 1);
                }
            }
            else
            {
                logger("No visgroups block found in VMF. Aborting fix.");
                return;
            }

            if (!hasVersioninfo)
            {
                string versioninfoBlock = "versioninfo\n{\n\t\"editorversion\" \"400\"\n\t\"editorbuild\" \"9999\"\n\t\"mapversion\" \"" + mapversion + "\"\n\t\"formatversion\" \"100\"\n\t\"prefab\" \"0\"\n}";
                lines.InsertRange(0, versioninfoBlock.Split('\n'));
            }

            if (visgroupsLines.Count > 0)
            {
                int insertIdx = hasVersioninfo ? 0 : 9; // Approx index after versioninfo if inserted
                // It's safer to find the end of versioninfo. But following C++ logic closely:
                int versionInfoEnd = -1;
                if (!hasVersioninfo) versionInfoEnd = 8;
                else
                {
                    for(int i=0; i<lines.Count; i++) {
                        if (lines[i].Trim() == "versioninfo") {
                            int brackets=0;
                            bool found=false;
                            for(int j=i; j<lines.Count; j++) {
                                brackets += lines[j].Count(c => c == '{');
                                brackets -= lines[j].Count(c => c == '}');
                                if(lines[j].Contains('{')) found=true;
                                if(found && brackets==0) {
                                    versionInfoEnd = j;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }

                int insertAt = versionInfoEnd != -1 ? versionInfoEnd + 1 : 0;
                lines.InsertRange(insertAt, visgroupsLines);
            }

            if (!hasViewsettings)
            {
                string viewsettingsBlock = "viewsettings\n{\n\t\"bSnapToGrid\" \"1\"\n\t\"bShowGrid\" \"1\"\n\t\"bShowLogicalGrid\" \"0\"\n\t\"nGridSpacing\" \"64\"\n\t\"bShow3DGrid\" \"0\"\n}";
                // Find visgroups end to insert after
                int visgroupsEnd = -1;
                for(int i=0; i<lines.Count; i++) {
                        if (lines[i].Trim() == "visgroups") {
                            int brackets=0;
                            bool found=false;
                            for(int j=i; j<lines.Count; j++) {
                                brackets += lines[j].Count(c => c == '{');
                                brackets -= lines[j].Count(c => c == '}');
                                if(lines[j].Contains('{')) found=true;
                                if(found && brackets==0) {
                                    visgroupsEnd = j;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                int insertAt = visgroupsEnd != -1 ? visgroupsEnd + 1 : 0;
                lines.InsertRange(insertAt, viewsettingsBlock.Split('\n'));
            }

            if (!hasCordon)
            {
                string cordonBlock = "cordon\n{\n\t\"mins\" \"(-1024 -1024 -1024)\"\n\t\"maxs\" \"(1024 1024 1024)\"\n\t\"active\" \"0\"\n}";
                lines.AddRange(cordonBlock.Split('\n'));
            }

            List<string> outLines = new List<string>();
            bool inDispinfo = false;
            int openBracketsDisp = 0;
            bool inDispinfoBracket = false;
            bool hasOffsets = false;

            foreach (var l in lines)
            {
                string trimmed = l.Trim();

                if (trimmed == "dispinfo")
                {
                    inDispinfo = true;
                    openBracketsDisp = 0;
                    inDispinfoBracket = false;
                    hasOffsets = false;
                }

                if (inDispinfo)
                {
                    openBracketsDisp += l.Count(c => c == '{');
                    openBracketsDisp -= l.Count(c => c == '}');
                    if (!inDispinfoBracket && l.Contains('{'))
                    {
                        inDispinfoBracket = true;
                    }

                    if (trimmed == "offsets" || trimmed == "offset_normals")
                    {
                        hasOffsets = true;
                    }

                    if (inDispinfoBracket && openBracketsDisp == 0)
                    {
                        inDispinfo = false;
                    }
                }

                if (inDispinfo && trimmed == "alphas" && !hasOffsets)
                {
                    string indent = new string(l.TakeWhile(char.IsWhiteSpace).ToArray());
                    string offsetsBlock =
                        indent + "offsets\n" +
                        indent + "{\n" +
                        indent + "\t\"row0\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row1\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row2\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row3\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row4\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row5\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row6\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row7\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "\t\"row8\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n" +
                        indent + "}\n" +
                        indent + "offset_normals\n" +
                        indent + "{\n" +
                        indent + "\t\"row0\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row1\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row2\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row3\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row4\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row5\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row6\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row7\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "\t\"row8\" \"0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1 0 0 1\"\n" +
                        indent + "}";
                    outLines.AddRange(offsetsBlock.Split('\n'));
                }

                outLines.Add(l);
            }

            File.WriteAllLines(vmfPath, outLines);
        }

        private static void CopyDirectory(string sourceDir, string destinationDir, bool recursive, LogCallback logger)
        {
            var dir = new DirectoryInfo(sourceDir);

            if (!dir.Exists)
                throw new DirectoryNotFoundException($"Source directory not found: {dir.FullName}");

            DirectoryInfo[] dirs = dir.GetDirectories();

            Directory.CreateDirectory(destinationDir);

            foreach (FileInfo file in dir.GetFiles())
            {
                string targetFilePath = Path.Combine(destinationDir, file.Name);
                file.CopyTo(targetFilePath, true);
            }

            if (recursive)
            {
                foreach (DirectoryInfo subDir in dirs)
                {
                    string newDestinationDir = Path.Combine(destinationDir, subDir.Name);
                    CopyDirectory(subDir.FullName, newDestinationDir, true, logger);
                }
            }
        }

        public static void ProcessBsp(Options options)
        {
            if (options.logger == null) return;

            string appDir = options.app_dir;
            string mapsDir = Path.Combine(appDir, "maps");
            Directory.CreateDirectory(mapsDir);

            string vmfDest = Path.Combine(mapsDir, options.map_name + ".vmf");
            string bspsrcJar = Path.Combine(appDir, "bspsrc.jar");

            if (!File.Exists(bspsrcJar))
            {
                throw new Exception($"Could not find bspsrc.jar at {bspsrcJar}");
            }

            options.logger("Decompiling BSP: " + options.bsp_file);

            string decompCmd = $"java -jar \"{bspsrcJar}\" \"{options.bsp_file}\" -o \"{vmfDest}\" --unpack_embedded";
            int ret = RunCommandSync(decompCmd, options.logger);
            if (ret != 0)
            {
                throw new Exception("BSP Decompilation failed.");
            }

            string unpackedDir = "";
            var possibleLocations = new List<string>
            {
                Path.Combine(Directory.GetCurrentDirectory(), options.map_name),
                Path.Combine(appDir, options.map_name),
                Path.Combine(Path.GetDirectoryName(options.bsp_file) ?? "", options.map_name),
                Path.Combine(mapsDir, options.map_name)
            };

            foreach (var loc in possibleLocations)
            {
                if (Directory.Exists(loc))
                {
                    unpackedDir = loc;
                    break;
                }
            }

            string targetUnpackedDir = Path.Combine(mapsDir, options.map_name);
            if (!string.IsNullOrEmpty(unpackedDir))
            {
                options.logger("Found unpacked files at " + unpackedDir);

                if (unpackedDir != targetUnpackedDir)
                {
                    if (Directory.Exists(targetUnpackedDir))
                    {
                        Directory.Delete(targetUnpackedDir, true);
                    }
                    try
                    {
                        Directory.Move(unpackedDir, targetUnpackedDir);
                        options.logger("Moved unpacked directory to " + targetUnpackedDir);
                    }
                    catch (Exception)
                    {
                        options.logger($"Failed to rename unpacked directory to {targetUnpackedDir}. Attempting copy/delete...");
                        CopyDirectory(unpackedDir, targetUnpackedDir, true, options.logger);
                        Directory.Delete(unpackedDir, true);
                    }
                }
            }
            else
            {
                options.logger($"Could not find unpacked embedded files directory '{options.map_name}'");
            }

            FixVmfFromBsp(vmfDest, options.logger);

            string targetMapsDir = Path.Combine(appDir, "maps", options.map_name, "maps");
            Directory.CreateDirectory(targetMapsDir);
            string finalVmfDest = Path.Combine(targetMapsDir, options.map_name + ".vmf");

            if (File.Exists(finalVmfDest))
            {
                File.Delete(finalVmfDest);
            }

            try
            {
                File.Move(vmfDest, finalVmfDest);
                options.logger("Moved VMF to: " + finalVmfDest);
            }
            catch (Exception)
            {
                options.logger("Failed to move VMF to: " + finalVmfDest);
            }

            options.content_folder = Path.Combine(appDir, "maps", options.map_name);
            options.logger("Decompiled and prepared at: " + finalVmfDest);

            string s1Subfolder = (options.s1_game_type == "css") ? "cstrike" : "csgo";
            string s1gamedir = Path.Combine(options.s1game_basefolder, s1Subfolder);

            if (Directory.Exists(targetUnpackedDir))
            {
                string srcMaterials = Path.Combine(targetUnpackedDir, "materials");
                string destMaterials = Path.Combine(s1gamedir, "materials");
                if (Directory.Exists(srcMaterials))
                {
                    options.logger("Copying materials to " + destMaterials);
                    CopyDirectory(srcMaterials, destMaterials, true, options.logger);
                }

                string srcModels = Path.Combine(targetUnpackedDir, "models");
                string destModels = Path.Combine(s1gamedir, "models");
                if (Directory.Exists(srcModels))
                {
                    options.logger("Copying models to " + destModels);
                    CopyDirectory(srcModels, destModels, true, options.logger);
                }
            }
        }
    }
}
