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

        if not self.java_installed:
            self.vmf_button.setToolTip('Java is not installed. BSP decompilation is disabled.')

        self.load_from_cfg()

        self.cs2_button.clicked.connect(self.select_cs2_folder)
        self.csgo_button.clicked.connect(self.select_csgo_folder)
        self.vmf_button.clicked.connect(self.select_vmf)
        self.validate_cs2_button.clicked.connect(self.validate_cs2)
        self.validate_csgo_button.clicked.connect(self.validate_csgo)
        self.addon_edit.textChanged.connect(self.get_addon)
        self.launch_options_edit.textChanged.connect(self.get_launch_options)
        self.go_button.clicked.connect(self.go)

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
        filter_str = "Map files (*.bsp *.vmf);;VMF files (*.vmf);;BSP files (*.bsp)" if self.java_installed else "VMF files (*.vmf)"
        path = QFileDialog.getOpenFileName(self, "Select a BSP or VMF", self.vmf_default_path, filter_str)[0]
        if not path:
            return
        
        self.bsp_file = None
        temp = path.split("/")
        filename = temp.pop()

        if filename.lower().endswith(".bsp"):
            self.bsp_file = path
            self.map_name = filename.split(".bsp")[0]
            self.vmf_folder = "/".join(temp)
            self.vmf_folder_to_save = self.vmf_folder

            self.vmf_label.setText(path)
            self.vmf_label.setStyleSheet("background-color:rgb(0, 255, 0)")
            return

        self.map_name = filename.split(".vmf")[0]
        self.vmf_folder = "/".join(temp)

        # if path doesnt end with /maps
        if not self.vmf_folder.endswith("/maps"):
            temp_dir = tempfile.gettempdir()

            # check if /maps is in temp already, otherwise create it
            if not os.path.exists(temp_dir + "/maps"):
                os.mkdir(temp_dir + "/maps")
            
            # delete vmf in /maps if exists, as maybe it isnt the newest ver. 
            # only need to do this if /maps wasnt just created.
            else:
                if os.path.isfile(temp_dir  + "/maps/" + self.map_name + ".vmf"):
                    os.remove(temp_dir  + "/maps/" + self.map_name + ".vmf")

            # copy *.vmf to temp/maps/*.vmf
            shutil.copy(self.vmf_folder + "/" + self.map_name + ".vmf", temp_dir + "/maps")
            
            self.vmf_folder_to_save = self.vmf_folder
            self.vmf_folder = temp_dir

        else:
            self.vmf_folder = "/".join(self.vmf_folder.split("/")[:-1])
            self.vmf_folder_to_save = self.vmf_folder
            print(self.vmf_folder)

        # update gui
        self.vmf_label.setText(path)
        self.vmf_label.setStyleSheet("background-color:rgb(0, 255, 0)")

    def get_addon(self):
        self.addon = self.addon_edit.text()

    def get_launch_options(self):
        self.launch_options = self.launch_options_edit.text()

    def set_launch_options(self, text):
        self.launch_options_edit.setText(text)

    def save_to_cfg(self): 
        temp = f"""{self.launch_options}
{self.cs2_basefolder}
{self.csgo_basefolder}
{self.vmf_folder_to_save}"""
        
        with open("cs2importer.cfg", "w") as f:
            f.write(temp)

    def load_from_cfg(self):
        if not os.path.isfile("cs2importer.cfg"):
            open("cs2importer.cfg", "w").close()

        with open("cs2importer.cfg", "r") as f:
            temp = f.readlines()
            if not temp:
                return

        self.set_launch_options(temp[0].strip())

        # Backwards compatibility check
        if len(temp) == 3:
            # Old format: launch_options, old_csgo_folder(which was CS2), vmf
            # We assume old format means they had Counter-Strike Global Offensive set.
            self.set_cs2_folder(temp[1].strip())
            self.vmf_default_path = temp[2].strip()
        elif len(temp) >= 4:
            # New format: launch_options, cs2_folder, csgo_folder, vmf
            self.set_cs2_folder(temp[1].strip())
            self.set_csgo_folder(temp[2].strip())
            self.vmf_default_path = temp[3].strip()

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

                temp_dir = tempfile.gettempdir()
                maps_dir = os.path.join(temp_dir, "maps")
                if not os.path.exists(maps_dir):
                    os.mkdir(maps_dir)

                vmf_dest = os.path.join(maps_dir, self.map_name + ".vmf")
                bspsrc_jar = resource_path("bspsrc.jar")

                if not os.path.exists(bspsrc_jar):
                    raise Exception(f"Could not find bspsrc.jar at {bspsrc_jar}")

                print(f"Decompiling BSP: {self.bsp_file}")
                # decompile using java
                decomp_cmd = ["java", "-jar", bspsrc_jar, self.bsp_file, "-out", vmf_dest]
                subprocess.check_call(decomp_cmd)

                self.vmf_folder = temp_dir
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
            subprocess.Popen(command, cwd=cd)

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
