Import("env")

from pathlib import Path
import shutil

project_dir = Path(env["PROJECT_DIR"])
patch_dir = project_dir / "patches" / "arduino_gfx_nv3007"
display_dir = (
    Path(env.subst("$PROJECT_LIBDEPS_DIR"))
    / env["PIOENV"]
    / "GFX Library for Arduino"
    / "src"
    / "display"
)

for filename in ("Arduino_NV3007.h", "Arduino_NV3007.cpp"):
    source = patch_dir / filename
    destination = display_dir / filename
    if not source.is_file():
        raise RuntimeError(f"Missing KAC-2.79 driver patch: {source}")
    if not destination.is_file():
        raise RuntimeError(f"Arduino_GFX NV3007 destination missing: {destination}")
    if source.read_bytes() != destination.read_bytes():
        shutil.copy2(source, destination)
        print(f"Applied KAC-2.79 NV3007 patch: {filename}")
