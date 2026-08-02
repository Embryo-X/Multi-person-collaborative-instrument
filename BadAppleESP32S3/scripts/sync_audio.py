Import("env")

from pathlib import Path
import shutil

project_dir = Path(env["PROJECT_DIR"])
data_dir = project_dir / "data"
piano_dir = data_dir / "piano"

source_dir = None
for candidate in (project_dir / "all_piano").iterdir():
    if candidate.is_dir() and len(list(candidate.glob("*.mp3"))) == 88:
        source_dir = candidate
        break
if source_dir is None:
    raise RuntimeError("Could not find the folder containing 88 piano MP3 files")

samples = sorted(source_dir.glob("*.mp3"), key=lambda path: int(path.name.split("-", 1)[0]))
piano_dir.mkdir(parents=True, exist_ok=True)

legacy_main = data_dir / "main.mp3"
if legacy_main.exists():
    legacy_main.unlink()

expected_names = {f"{index:02d}.mp3" for index in range(1, 89)}
for stale in piano_dir.glob("*.mp3"):
    if stale.name not in expected_names:
        stale.unlink()

copied = 0
for index, source in enumerate(samples, start=1):
    destination = piano_dir / f"{index:02d}.mp3"
    if not destination.exists() or destination.stat().st_size != source.stat().st_size:
        shutil.copy2(source, destination)
        copied += 1

print(f"Piano samples ready: 88 files ({copied} updated)")
