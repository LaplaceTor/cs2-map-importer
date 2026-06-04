using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;

namespace CS2Importer
{
    public class MapImporter
    {
        public class Options
        {
            public string s1gamedir = "";
            public string s1gamename = "";
            public string s1contentdir = "";
            public string s2addonname = "";
            public string s2contentdir = "";
            public string mapname = "";
            public bool usebsp = false;
            public bool usebsp_nomergeinstances = false;
            public bool skipdeps = false;
            public string cs2_basefolder = "";
        }

        private Options m_options;
        private AppCore.LogCallback? m_log;

        public MapImporter(Options options, AppCore.LogCallback? logger)
        {
            m_options = options;
            m_log = logger;
        }

        private void Log(string msg)
        {
            m_log?.Invoke(msg);
        }

        private int RunCommand(string cmd)
        {
            if (m_log == null) return -1;
            return AppCore.RunCommandSync(cmd, m_log);
        }

        private string CleanRefPath(string input)
        {
            int filePos = input.IndexOf("\"file\"");
            if (filePos != -1)
            {
                input = input.Substring(filePos + 6);
            }
            input = input.Trim(' ', '\t', '"');
            if (input == "importfilelist" || input == "{" || input == "}") return "";
            return input;
        }

        private List<string> ReadTextFile(string filepath)
        {
            if (File.Exists(filepath))
            {
                return File.ReadAllLines(filepath).ToList();
            }
            return new List<string>();
        }

        private void EnsureFileWritable(string filepath)
        {
            string? dir = Path.GetDirectoryName(filepath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }
            if (File.Exists(filepath))
            {
                var attrs = File.GetAttributes(filepath);
                if (attrs.HasFlag(FileAttributes.ReadOnly))
                {
                    File.SetAttributes(filepath, attrs & ~FileAttributes.ReadOnly);
                }
            }
        }

        private void StripMDLsFromRefs(string filename)
        {
            var refs = ReadTextFile(filename);
            var mdls = new List<string>();
            var others = new List<string>();

            foreach (var r in refs)
            {
                if (string.IsNullOrEmpty(r)) continue;
                string cleanedRef = CleanRefPath(r);
                if (string.IsNullOrEmpty(cleanedRef)) continue;

                if (cleanedRef.ToLower().Contains(".mdl"))
                {
                    mdls.Add(cleanedRef);
                }
                else
                {
                    others.Add(cleanedRef);
                }
            }

            string mdlfilename = filename.Replace("_refs.txt", "_mdl_lst.txt");
            EnsureFileWritable(mdlfilename);
            using (var sw = new StreamWriter(mdlfilename))
            {
                sw.WriteLine("importfilelist\n{");
                foreach (var m in mdls) sw.WriteLine($"\t\"file\"\t\"{m}\"");
                sw.WriteLine("}");
            }

            string refsfilename = filename.Replace("_refs.txt", "_new_refs.txt");
            EnsureFileWritable(refsfilename);
            using (var sw = new StreamWriter(refsfilename))
            {
                sw.WriteLine("importfilelist\n{");
                foreach (var o in others) sw.WriteLine($"\t\"file\"\t\"{o}\"");
                sw.WriteLine("}");
            }
        }

        private void ForceUV2ForVMAT(string mtlfile)
        {
            string vmat = mtlfile.Replace(".vmt", ".vmat");
            string vmatfilename = Path.Combine(m_options.s2contentdir, vmat);

            if (!File.Exists(vmatfilename)) return;

            var lines = ReadTextFile(vmatfilename);
            EnsureFileWritable(vmatfilename);

            bool added = false;
            for (int i = 0; i < lines.Count; i++)
            {
                string lowerTxt = lines[i].TrimStart().ToLower();
                if (lowerTxt.StartsWith("\"shader\""))
                {
                    if (i + 1 < lines.Count)
                    {
                        string lowerNext = lines[i + 1].TrimStart().ToLower();
                        if (!lowerNext.StartsWith("\"f_force_uv2\""))
                        {
                            lines.Insert(i + 1, "\t\"F_FORCE_UV2\" \"1\"");
                            added = true;
                            break;
                        }
                    }
                }
            }

            if (added)
            {
                Log("Added F_FORCE_UV2 to " + vmatfilename);
                File.WriteAllLines(vmatfilename, lines);
            }
        }

        private bool Force2UVsIfRequired(string refsName, HashSet<string> global2UVMaterials, string global2UVMaterialsFilepath)
        {
            var uvsUpdated = new HashSet<string>();
            string meshinfofilename = refsName.Replace("_refs.txt", "_refs\\mesh\\meshinfo.txt");

            if (!File.Exists(meshinfofilename)) return false;

            string meshstring = File.ReadAllText(meshinfofilename);
            bool b2UV = false;

            if (!File.Exists(refsName)) return false;

            var refsList = ReadTextFile(refsName);
            int numuvs = 1;
            if (meshstring.Contains("'numuvs': 2") || meshstring.Contains("\"numuvs\": 2"))
            {
                numuvs = 2;
            }

            foreach (var refLine in refsList)
            {
                string mtlfile = CleanRefPath(refLine);
                if (string.IsNullOrEmpty(mtlfile)) continue;
                if (uvsUpdated.Contains(mtlfile)) continue;

                if (global2UVMaterials.Contains(mtlfile))
                {
                    b2UV = true;
                    uvsUpdated.Add(mtlfile);
                }
                else
                {
                    if (numuvs == 2)
                    {
                        b2UV = true;
                        Log("Adding F_FORCE_UV2 to mtls imported from " + refsName + "...");
                        uvsUpdated.Add(mtlfile);
                        global2UVMaterials.Add(mtlfile);

                        EnsureFileWritable(global2UVMaterialsFilepath);
                        File.AppendAllText(global2UVMaterialsFilepath, mtlfile + "\n");

                        ForceUV2ForVMAT(mtlfile);
                    }
                }
            }

            return b2UV;
        }

        private void ImportAndCompileMapMDLs(string filename)
        {
            var mdlfiles = ReadTextFile(filename);
            if (mdlfiles.Count == 0)
            {
                Log("No MDLs to import");
                return;
            }

            Log("Importing models");
            Log("--------------------------------");
            foreach (var x in mdlfiles)
            {
                if (string.IsNullOrEmpty(x) || x.StartsWith("-")) continue;
                Log(x);
            }
            Log("--------------------------------");

            var force2UVList = new List<string>();
            var mdlmtls = new HashSet<string>();
            string extraoptions = "";

            foreach (var m in mdlfiles)
            {
                if (string.IsNullOrEmpty(m)) continue;
                if (m.StartsWith("-"))
                {
                    if (m == "-" || m == "-nooptions") extraoptions = "";
                    else extraoptions = m;
                }
                else
                {
                    string mdlfile = CleanRefPath(m).Replace("/", "\\");
                    if (string.IsNullOrEmpty(mdlfile)) continue;

                    string infile = mdlfile;
                    string outName = Path.Combine(m_options.s2contentdir, mdlfile).Replace(".mdl", ".vmdl");
                    string refsName = Path.Combine(m_options.s2contentdir, mdlfile).Replace(".mdl", "_refs.txt");

                    string importCmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\cs_mdl_import.exe\" -nop4 {extraoptions} -i \"{m_options.s1gamedir}\" -o \"{m_options.s2contentdir}\" \"{infile}\"";
                    RunCommand(importCmd);

                    if (File.Exists(refsName))
                    {
                        var refs = ReadTextFile(refsName);
                        foreach (var r in refs)
                        {
                            string cleanedRef = CleanRefPath(r);
                            if (!string.IsNullOrEmpty(cleanedRef)) mdlmtls.Add(cleanedRef);
                        }
                        force2UVList.Add(refsName);
                    }
                }
            }

            string temp_refs = filename.Replace("mdl_lst", "mtl_lst");
            EnsureFileWritable(temp_refs);
            using (var fw = new StreamWriter(temp_refs))
            {
                fw.WriteLine("importfilelist\n{");
                foreach (var mtl in mdlmtls) fw.WriteLine($"\t\"file\"\t\"{mtl}\"");
                fw.WriteLine("}");
            }

            string importRefsCmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"{m_options.s1gamedir}\" -s2addon {m_options.s2addonname} -game {m_options.s1gamename} -usefilelist \"{temp_refs}\"";
            RunCommand(importRefsCmd);

            var global2UVMaterials = new HashSet<string>();
            string global2UVMaterialFilepath = "source1import_2uvmateriallist.txt";
            if (File.Exists(global2UVMaterialFilepath))
            {
                var force2UVListFile = ReadTextFile(global2UVMaterialFilepath);
                foreach (var mtl in force2UVListFile)
                {
                    global2UVMaterials.Add(mtl);
                    ForceUV2ForVMAT(mtl);
                }
            }

            foreach (var mtlfile in mdlmtls)
            {
                if (string.IsNullOrEmpty(mtlfile) || mtlfile.StartsWith("-")) continue;
                string mtl = mtlfile.Replace("/", "\\");
                string outName = Path.Combine(m_options.s2contentdir, mtl).Replace(".vmt", ".vmat");

                string resCompCmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game {m_options.s1gamename} \"{outName}\"";
                RunCommand(resCompCmd);
            }

            foreach (var m in mdlfiles)
            {
                if (string.IsNullOrEmpty(m) || m.StartsWith("-")) continue;
                string mdlfile = CleanRefPath(m).Replace("/", "\\");
                if (string.IsNullOrEmpty(mdlfile)) continue;

                string outName = Path.Combine(m_options.s2contentdir, mdlfile).Replace(".mdl", ".vmdl");
                if (!File.Exists(outName)) continue;

                string refsName = Path.Combine(m_options.s2contentdir, mdlfile).Replace(".mdl", "_refs.txt");
                bool bForceCompile = Force2UVsIfRequired(refsName, global2UVMaterials, global2UVMaterialFilepath);

                string fFlag = bForceCompile ? "-f " : "";
                string resCompCmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 {fFlag}-game {m_options.s1gamename} \"{outName}\"";
                RunCommand(resCompCmd);
            }
        }

        private void ImportAndCompileMapRefs(string refsFile)
        {
            string importcmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync -src1gameinfodir \"{m_options.s1gamedir}\" -s2addon {m_options.s2addonname} -game {m_options.s1gamename} -usefilelist \"{refsFile}\"";
            RunCommand(importcmd);

            var refs = ReadTextFile(refsFile);
            string newList = "";

            foreach (var line in refs)
            {
                string cleanedRef = CleanRefPath(line);
                if (!string.IsNullOrEmpty(cleanedRef))
                {
                    string modLine = cleanedRef.Replace(".vmt", ".vmat").Replace(' ', '_').Replace('/', '\\');
                    newList += Path.Combine(m_options.s2contentdir, modLine) + "\n";
                }
            }

            string tmpFile = Path.Combine(m_options.s2contentdir, "maps", m_options.mapname + "_compile_new_refs.txt");
            EnsureFileWritable(tmpFile);
            File.WriteAllText(tmpFile, newList);

            string compilercmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\resourcecompiler.exe\" -retail -nop4 -game {m_options.s1gamename} -f -filelist \"{tmpFile}\"";
            RunCommand(compilercmd);
        }

        public bool Run()
        {
            Log("Starting Map Import process via C#.");

            string usebspStr = m_options.usebsp ? "-usebsp" : "";
            string nomergeinstancesStr = m_options.usebsp_nomergeinstances ? "-usebsp_nomergeinstances" : "";

            string mapImportCmd = $"\"{m_options.cs2_basefolder}\\game\\bin\\win64\\source1import.exe\" -retail -nop4 -nop4sync {usebspStr}";
            if (!string.IsNullOrEmpty(nomergeinstancesStr)) mapImportCmd += $" {nomergeinstancesStr}";
            mapImportCmd += $" -src1gameinfodir \"{m_options.s1gamedir}\" -src1contentdir \"{m_options.s1contentdir}\" -s2addon \"{m_options.s2addonname}\" -game {m_options.s1gamename} maps\\{m_options.mapname}.vmf";

            RunCommand(mapImportCmd);

            string mappedName = m_options.mapname.Replace("instances", "prefabs");

            if (!m_options.skipdeps)
            {
                StripMDLsFromRefs(Path.Combine(m_options.s2contentdir, "maps", mappedName + "_refs.txt"));
                ImportAndCompileMapMDLs(Path.Combine(m_options.s2contentdir, "maps", mappedName + "_mdl_lst.txt"));
                ImportAndCompileMapRefs(Path.Combine(m_options.s2contentdir, "maps", mappedName + "_new_refs.txt"));

                RunCommand(mapImportCmd);
            }

            Log("Import process complete.");
            return true;
        }
    }
}
