from ui.interface import Ui_MainWindow as Interface
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *

import re
import traceback
import sys
import subprocess
import os
import shutil
import tempfile
import urllib.request
import json
import zipfile
import io

def download_bspsrc(base_path):
    bspsrc_path = os.path.join(base_path, "bspsrc.jar")
    if os.path.exists(bspsrc_path):
        return True

    print("bspsrc.jar not found. Downloading the latest version...")
    try:
        download_url = "https://github.com/ata4/bspsrc/releases/latest/download/bspsrc-jar-only.zip"
        print(f"Downloading from {download_url}...")
        req_zip = urllib.request.Request(download_url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req_zip) as zip_response:
            with zipfile.ZipFile(io.BytesIO(zip_response.read())) as zip_file:
                zip_file.extract("bspsrc.jar", path=base_path)

        print(f"Successfully downloaded and extracted bspsrc.jar to {base_path}")
        return True
    except Exception as e:
        print(f"Failed to download bspsrc: {e}")
        return False

def resource_path(relative_path):
    """ Get absolute path to resource, works for dev and for PyInstaller """
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

def check_colorama():
    try:
        # Check if the system python (the one used for Valve scripts) has colorama
        subprocess.check_call(["python", "-c", "import colorama"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        print("colorama not found in system python. Installing...")
        try:
            subprocess.check_call(["python", "-m", "pip", "install", "colorama"])
        except Exception as e:
            print(f"Failed to install colorama: {e}")

def check_java():
    try:
        result = subprocess.run(["java", "-version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return True
    except FileNotFoundError:
        return False

class Importer(QMainWindow, Interface):
    def __init__(self):
        super().__init__()

        check_colorama()

        # Get the directory where the executable or script is located
        if getattr(sys, 'frozen', False):
            self.app_dir = os.path.dirname(sys.executable)
        else:
            self.app_dir = os.path.abspath(".")

        self.bspsrc_installed = download_bspsrc(self.app_dir)

        self.vmf_default_path = "C:\\"
        self.cs2_basefolder = None
        self.csgo_basefolder = None
        self.vmf_folder = None
        self.vmf_folder_to_save = "C:\\"
        self.addon = None
        self.map_name = None
        self.vpk_signatures_moved = False
        self.java_installed = check_java()
        self.bsp_file = None

        self.setupUi(self)
        self.set_tooltips()
        self.set_stylesheets()
        self.get_addon()
        self.get_launch_options()

        if not self.java_installed or not self.bspsrc_installed:
            self.bsp_button.setToolTip('Java or bspsrc.jar is missing. BSP decompilation is disabled.')
            self.bsp_button.setEnabled(False)

        self.load_from_cfg()

        self.cs2_button.clicked.connect(self.select_cs2_folder)
        self.csgo_button.clicked.connect(self.select_csgo_folder)
        self.vmf_button.clicked.connect(self.select_vmf)
        self.bsp_button.clicked.connect(self.select_bsp)
        self.validate_cs2_button.clicked.connect(self.validate_cs2)
        self.validate_csgo_button.clicked.connect(self.validate_csgo)
        self.addon_edit.textChanged.connect(self.get_addon)
        self.go_button.clicked.connect(self.go)

        # Connect mutual exclusivity for usebsp checkboxes
        self.usebsp_checkbox.toggled.connect(self.on_usebsp_toggled)
        self.usebsp_nomergeinstances_checkbox.toggled.connect(self.on_usebsp_nomergeinstances_toggled)

        # Connect checkboxes to get_launch_options
        self.usebsp_checkbox.stateChanged.connect(self.get_launch_options)
        self.usebsp_nomergeinstances_checkbox.stateChanged.connect(self.get_launch_options)
        self.skipdeps_checkbox.stateChanged.connect(self.get_launch_options)

    def validate_cs2(self):
        # Open URL to prompt Steam to validate CS2 files
        os.system("start steam://validate/730")

    def validate_csgo(self):
        # Open URL to prompt Steam to validate CSGO files
        os.system("start steam://validate/4465480")

    def set_stylesheets(self):
        self.cs2_label.setStyleSheet("background-color:rgb(255, 0, 0)")
        self.csgo_label.setStyleSheet("background-color:rgb(255, 0, 0)")
        self.vmf_label.setStyleSheet("background-color:rgb(255, 0, 0)")

    def set_tooltips(self):
        self.cs2_button.setToolTip('Use "Counter-Strike Global Offensive" folder or any folder inside it.')
        self.csgo_button.setToolTip('Use "csgo legacy" folder or any folder inside it.')
        self.vmf_button.setToolTip('Does not need to be in a "maps" folder, one will be created then deleted afterwards if necessary.')
        self.config_checkbox.setToolTip('Auto-selects folders, auto-selects .VMF folder when you open the dialog, and auto-fills launch options for next time.')
        self.usebsp_checkbox.setToolTip('This runs the map through a special vbsp process to generate clean map geometry from brushes, removing hidden faces and stitching up edges, making the CS2 version easier to work with in Hammer. It preserves world (vis) brushes and func_detail brushes for compatibility with Source 2. This parameter will also merge all func_instances in your map. Note that the final geometry will be triangulated, but cleaning it up is a fairly simple process, which will be explained in another guide.')
        self.usebsp_nomergeinstances_checkbox.setToolTip('Use this instead of -usebsp if you wish to both generate clean geo and also preserve func_instances. Note that this takes a little longer as it has to run through the import process twice. The final geometry will also be triangulated.')
        self.skipdeps_checkbox.setToolTip("Optional: skips importing all dependencies/content and only generates the vmap file(s). This provides a 'quick' import when iterating entities for example. Do not run with this if you are importing for the first time.")

    def select_cs2_folder(self):
        path = QFileDialog.getExistingDirectory(self, "Select a folder:", "C:\\", QFileDialog.ShowDirsOnly)
        if not path:
            return
        
        path = re.split("(/Counter-Strike Global Offensive/)", path)
        path.append("") # to add a second element incase there isnt one, occurs if selected base folder not a subfolder i.e. csgo/cfg
        
        path = path[0] + path[1]
        self.set_cs2_folder(path)

    def set_cs2_folder(self, path):
        if path and path != "None":
            self.cs2_basefolder = path
            self.cs2_label.setText(path)
            self.cs2_label.setStyleSheet("background-color:rgb(0, 255, 0)")

    def select_csgo_folder(self):
        path = QFileDialog.getExistingDirectory(self, "Select a folder:", "C:\\", QFileDialog.ShowDirsOnly)
        if not path:
            return

        path = re.split("(/csgo legacy/)", path)
        path.append("") # to add a second element incase there isnt one

        path = path[0] + path[1]
        self.set_csgo_folder(path)

    def set_csgo_folder(self, path):
        if path and path != "None":
            self.csgo_basefolder = path
            self.csgo_label.setText(path)
            self.csgo_label.setStyleSheet("background-color:rgb(0, 255, 0)")

    def select_vmf(self):
        filter_str = "VMF files (*.vmf)"
        path = QFileDialog.getOpenFileName(self, "Select a VMF", self.vmf_default_path, filter_str)[0]
        if not path:
            return
        
        self.bsp_file = None
        temp = path.split("/")
        filename = temp.pop()

        self.map_name = filename.split(".vmf")[0]
        self.vmf_folder = "/".join(temp)

        # Always copy the selected VMF to self.app_dir/maps so it behaves exactly like BSP decompilation
        target_maps_dir = os.path.join(self.app_dir, "maps")
        if not os.path.exists(target_maps_dir):
            os.mkdir(target_maps_dir)
            
        target_vmf_path = os.path.join(target_maps_dir, self.map_name + ".vmf")
        source_vmf_path = os.path.join(self.vmf_folder, self.map_name + ".vmf")

        if not os.path.samefile(source_vmf_path, target_vmf_path) if os.path.exists(target_vmf_path) else True:
            if os.path.exists(target_vmf_path):
                os.remove(target_vmf_path)
            shutil.copy(source_vmf_path, target_maps_dir)

        # import_map_community.py expects the parent of the maps folder
        self.vmf_folder_to_save = self.vmf_folder
        self.vmf_folder = self.app_dir
        print(f"VMF set up at: {target_vmf_path}")

        # update gui
        self.vmf_label.setText(path)
        self.vmf_label.setStyleSheet("background-color:rgb(0, 255, 0)")

    def select_bsp(self):
        filter_str = "BSP files (*.bsp)"
        path = QFileDialog.getOpenFileName(self, "Select a BSP", self.vmf_default_path, filter_str)[0]
        if not path:
            return

        self.bsp_file = path
        temp = path.split("/")
        filename = temp.pop()

        self.map_name = filename.split(".bsp")[0]
        self.vmf_folder_to_save = "/".join(temp)

        # update gui
        self.vmf_label.setText(path)
        self.vmf_label.setStyleSheet("background-color:rgb(0, 255, 0)")

    def on_usebsp_toggled(self, checked):
        if checked:
            self.usebsp_nomergeinstances_checkbox.setChecked(False)

    def on_usebsp_nomergeinstances_toggled(self, checked):
        if checked:
            self.usebsp_checkbox.setChecked(False)

    def get_addon(self):
        self.addon = self.addon_edit.text()

    def get_launch_options(self):
        options = []
        if self.usebsp_checkbox.isChecked():
            options.append("-usebsp")
        if self.usebsp_nomergeinstances_checkbox.isChecked():
            options.append("-usebsp_nomergeinstances")
        if self.skipdeps_checkbox.isChecked():
            options.append("-skipdeps")
        self.launch_options = " ".join(options)

    def save_to_cfg(self): 
        usebsp_state = str(self.usebsp_checkbox.isChecked())
        nomerge_state = str(self.usebsp_nomergeinstances_checkbox.isChecked())
        skipdeps_state = str(self.skipdeps_checkbox.isChecked())

        temp = f"""{usebsp_state}
{nomerge_state}
{skipdeps_state}
{self.cs2_basefolder}
{self.csgo_basefolder}
{self.vmf_folder_to_save}"""
        
        with open("cs2importer.cfg", "w") as f:
            f.write(temp)

    def load_from_cfg(self):
        if not os.path.isfile("cs2importer.cfg"):
            open("cs2importer.cfg", "w").close()

        with open("cs2importer.cfg", "r") as f:
            temp = [line.strip() for line in f.readlines()]
            if not temp:
                return

        # Check if new format (first line is True or False)
        if temp[0] in ["True", "False"]:
            if len(temp) >= 6:
                self.usebsp_checkbox.setChecked(temp[0] == "True")
                self.usebsp_nomergeinstances_checkbox.setChecked(temp[1] == "True")
                self.skipdeps_checkbox.setChecked(temp[2] == "True")
                self.set_cs2_folder(temp[3])
                self.set_csgo_folder(temp[4])
                self.vmf_default_path = temp[5]
        else:
            # Old format backwards compatibility check
            # We ignore the old launch options (temp[0])
            if len(temp) == 3:
                # Old format: launch_options, old_csgo_folder(which was CS2), vmf
                # We assume old format means they had Counter-Strike Global Offensive set.
                self.set_cs2_folder(temp[1])
                self.vmf_default_path = temp[2]
            elif len(temp) >= 4:
                # New format (but still string launch options): launch_options, cs2_folder, csgo_folder, vmf
                self.set_cs2_folder(temp[1])
                self.set_csgo_folder(temp[2])
                self.vmf_default_path = temp[3]

    def fix_import_script(self):
        if not self.cs2_basefolder:
            return

        script_path = os.path.join(self.cs2_basefolder, 'game', 'csgo', 'import_scripts', 'import_map_community.py')
        if not os.path.exists(script_path):
            return

        with open(script_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        if len(lines) >= 328:
            if '.decode()' in lines[327]:
                lines[327] = lines[327].replace('.decode()', '')
                with open(script_path, 'w', encoding='utf-8') as f:
                    f.writelines(lines)

    def move_vpk_signatures(self):
        if not self.cs2_basefolder:
            return

        bin_folder = os.path.join(self.cs2_basefolder, 'game', 'bin', 'win64')
        vpk_path = os.path.join(bin_folder, 'vpk.signatures')
        temp_folder = os.path.join(bin_folder, 'temp')
        temp_vpk_path = os.path.join(temp_folder, 'vpk.signatures')

        if os.path.exists(vpk_path):
            if not os.path.exists(temp_folder):
                os.makedirs(temp_folder)

            # Use shutil.move to handle overwriting if target exists
            if os.path.exists(temp_vpk_path):
                os.remove(temp_vpk_path)
            shutil.move(vpk_path, temp_vpk_path)
            self.vpk_signatures_moved = True

    def go(self):
        try:
            if bool(self.config_checkbox.checkState()):
                self.save_to_cfg()

            self.fix_import_script()
            self.move_vpk_signatures()

            if self.bsp_file:
                if not self.java_installed:
                    raise Exception("Java is not installed. Cannot decompile BSP file.")

                # Use the app directory and create maps folder
                maps_dir = os.path.join(self.app_dir, "maps")
                if not os.path.exists(maps_dir):
                    os.mkdir(maps_dir)

                vmf_dest = os.path.join(maps_dir, self.map_name + ".vmf")
                bspsrc_jar = os.path.join(self.app_dir, "bspsrc.jar")

                if not os.path.exists(bspsrc_jar):
                    raise Exception(f"Could not find bspsrc.jar at {bspsrc_jar}")

                print(f"Decompiling BSP: {self.bsp_file}")
                # decompile using java
                decomp_cmd = ["java", "-jar", bspsrc_jar, self.bsp_file, "-o", vmf_dest, "--unpack_embedded"]
                subprocess.check_call(decomp_cmd)

                # Find the unpacked directory (named after map_name)
                # It could be created in the current working directory, or next to the BSP file, or next to the output VMF.
                unpacked_dir = None
                possible_locations = [
                    os.path.join(os.getcwd(), self.map_name),
                    os.path.join(self.app_dir, self.map_name),
                    os.path.join(os.path.dirname(self.bsp_file), self.map_name),
                    os.path.join(maps_dir, self.map_name) # already where it needs to be
                ]

                for loc in possible_locations:
                    if os.path.isdir(loc):
                        unpacked_dir = loc
                        break

                if unpacked_dir:
                    print(f"Found unpacked files at {unpacked_dir}")

                    # Copy materials and models to csgo_basefolder/csgo/ if they exist
                    if self.csgo_basefolder:
                        for folder_name in ["materials", "models"]:
                            src_folder = os.path.join(unpacked_dir, folder_name)
                            if os.path.isdir(src_folder):
                                dest_folder = os.path.join(self.csgo_basefolder, "csgo", folder_name)
                                print(f"Copying {src_folder} to {dest_folder}")
                                shutil.copytree(src_folder, dest_folder, dirs_exist_ok=True)

                    # Move the unpacked folder next to the generated VMF (inside maps_dir)
                    target_unpacked_dir = os.path.join(maps_dir, self.map_name)
                    if unpacked_dir != target_unpacked_dir:
                        if os.path.exists(target_unpacked_dir):
                            shutil.rmtree(target_unpacked_dir)
                        shutil.move(unpacked_dir, target_unpacked_dir)
                        print(f"Moved unpacked directory to {target_unpacked_dir}")
                else:
                    print(f"Could not find unpacked embedded files directory '{self.map_name}'")

                self.vmf_folder = self.app_dir
                print(f"Decompiled to: {vmf_dest}")

            cd = self.cs2_basefolder + '/game/csgo/import_scripts'
            command = "python import_map_community.py "
            command += '"' + self.csgo_basefolder + '/csgo' + '" '
            command += '"' + self.vmf_folder + '" '
            command += '"' + self.cs2_basefolder + '/game/csgo' + '" '
            command += self.addon + ' '
            command += self.map_name + ' '
            command += self.launch_options
            command = command.replace("/", "\\")
            print(command)

            my_env = os.environ.copy()
            bin_path = os.path.join(self.cs2_basefolder, 'game', 'bin', 'win64').replace("/", "\\")
            my_env["PATH"] = bin_path + os.pathsep + my_env.get("PATH", "")

            subprocess.Popen(command, cwd=cd, env=my_env)

        except Exception as e:
            print(e)
            QMessageBox.critical(self, "Error", str(traceback.format_exc()))

    def closeEvent(self, event):
        if self.vpk_signatures_moved and self.cs2_basefolder:
            bin_folder = os.path.join(self.cs2_basefolder, 'game', 'bin', 'win64')
            vpk_path = os.path.join(bin_folder, 'vpk.signatures')
            temp_vpk_path = os.path.join(bin_folder, 'temp', 'vpk.signatures')
            if os.path.exists(temp_vpk_path):
                if os.path.exists(vpk_path):
                    os.remove(vpk_path)
                shutil.move(temp_vpk_path, vpk_path)
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    imp = Importer()
    imp.show()
    sys.exit(app.exec_())
