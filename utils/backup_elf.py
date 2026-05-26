########################################################################################################################################################################
#
# This file backs up current firmware elf to backup folder.
# It is useful when you have crash dump from some specific version, and you need to find corresponding ELF file to debug it.
#
########################################################################################################################################################################

import datetime
import os
import shutil
import sys
import time
from pathlib import Path
from build_dir import find_latest_build_dir

project_root = Path(__file__).resolve().parent.parent
os.chdir(project_root)

build_dir = find_latest_build_dir()
if build_dir is None:
    print('No build/*/firmware found; nothing to back up.')
    sys.exit(0)

src_file = build_dir / 'firmware'
dst_folder = build_dir / 'debug_elf_backups'

# Get the current timestamp in the format "YYYY-MM-DD_HH-MM-SS"
timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

# Check if the directory already exists
if not dst_folder.exists():
    dst_folder.mkdir(parents=True)

# The new name for the copied file
dst_file = dst_folder / f"firmware_{timestamp}.elf"

# Copy the file and add timestamp to the name
shutil.copy2(src_file, dst_file)

print(f"Back up current ELF to {dst_file}")

## ERASE older files then 3 days, to avoid this taking up way to much space

# The current time in seconds since the epoch
now = time.time()

# The age threshold for files to delete (3 days in seconds)
age_threshold = 3 * 24 * 60 * 60

# List the files in the dst_folder
for file in os.listdir(dst_folder):
    # Get the full path of the file
    file_path = os.path.join(dst_folder, file)

    # Check if the file is a regular file and if its age exceeds the threshold
    if os.path.isfile(
            file_path) and now - os.path.getctime(file_path) > age_threshold:
        os.remove(file_path)
