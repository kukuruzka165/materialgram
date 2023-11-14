# new version preparation script
# libraries
import os, shutil
from datetime import date

# config
config = {
    "make_setup": True,               # set True if you want to make setup version
    "make_portable": True,            # set True if you want to make portable version
    "ota_enabled": True,              # set True if ota enabled
    "repo_path": "",                  # leave it blank if this script located in repo folder
    "version": "",                    # leave it blank to fill with version from SourceFiles/core/version.h and script runtime date
    "tgversion": "",                  # leave it blanl to set it automatically
    "exe_filename": "materialgram.exe", # set your client name here
}

# functions
def log(text, level):
    print("#" + " " * level + text)

def set_repo_path():
    log("Setting repo path...", 1)
    config["repo_path"] = os.getcwd()
    log("Repo path was not set in config", 2)
    log("Set repo path to: " + config["repo_path"], 2)

def set_version():
    log("Setting version...", 1)
    version_file = open(config["repo_path"] + "/Telegram/SourceFiles/core/version.h", "r")
    version_code = version_file.readlines()[25]
    config["version"] += version_code.replace('constexpr auto AppVersionStr = "', '').replace('";', date.today().strftime("-%d%m%Y")).replace('\n', '')
    config["tgversion"] += version_code.replace('constexpr auto AppVersionStr = "', '').replace('";', '').replace('\n', '')    

def set_iss():
    log("Updating iss file...", 1)
    iss_file = open(config["repo_path"] + "/Telegram/build/setup.iss", "r", encoding="utf-8")
    iss_file_data = iss_file.readlines()
    iss_file_data[3] = '#define MyAppVersion "' + config["tgversion"] + '"\n'
    iss_file_data[10] = '#define MyAppVersionFull "' + config["version"] + '"\n'
    iss_file.close()
    iss_file = open(config["repo_path"] + "/Telegram/build/setup.iss", "w", encoding="utf-8")
    iss_file.writelines(iss_file_data)

def check_files():
    log("# Renaming files...", 1)

    if not os.path.exists(os.path.join(config["repo_path"] + "/out/Release/", config["exe_filename"])):
        log(config["exe_filename"] + " does not exist too, halt...", 2)
        exit()

def run_iss_build():
    log("Running iscc build... If error occurs, check if iscc.exe path added to PATH", 1)
    os.system("iscc " + config["repo_path"] + "/Telegram/build/setup.iss")

def make_portable():
    log("Making portable version", 1)
    log("Creating 'portable' folder", 2)
    try:
        os.mkdir(os.path.join(config["repo_path"] + "/out/Release/portable"))
        log("Created 'portable' folder", 3)
    except:
        log("Folder 'portable' already exists", 3)
    log("Copying portable files", 2)
    try:
        shutil.copyfile(os.path.join(config["repo_path"] + "/out/Release/", config["exe_filename"]), os.path.join(config["repo_path"] + "/out/Release/portable", config["exe_filename"]))
        if config["ota_enabled"]:
            shutil.copyfile(os.path.join(config["repo_path"] + "/out/Release/", "Updater.exe"), os.path.join(config["repo_path"] + "/out/Release/portable", "Updater.exe"))
        shutil.copytree(config["repo_path"] + "/out/Release/modules", config["repo_path"] + "/out/Release/portable/modules")
        log("Files copied to 'portable' folder", 3)
    except:
        log("Files already exist", 3)
    log("Making archive...", 2)
    shutil.make_archive(os.path.join(config["repo_path"] + "/out/Release/releases/rtgdrelease-" + config["version"] + "/rtgdportable-x64." + config["version"]), 'zip', os.path.join(config["repo_path"] + "/out/Release/portable"))

if config["repo_path"] == "":
    set_repo_path()
if config["version"] == "":
    set_version()
check_files()
if config["make_setup"]:
    set_iss()
    run_iss_build()
if config["make_portable"]:
    make_portable()

log("All done!", 1)