"""PlatformIO pre-build: pack checked-in CSS/JS with Python's standard library."""
from pathlib import Path
import importlib.util
Import("env")
path=Path(env.subst('$PROJECT_DIR')).parent/'scripts/build_web_assets.py'
spec=importlib.util.spec_from_file_location('coloplay_web_assets',path)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.build_assets()
