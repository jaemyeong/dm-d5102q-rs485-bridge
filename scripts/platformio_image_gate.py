"""PlatformIO post-build hook for the product's stricter image-size gate."""

from pathlib import Path
import sys

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]
sys.path.insert(0, str(PROJECT_DIR / "scripts"))

from validate_memory_budget import (  # noqa: E402
    BudgetValidationError,
    validate_firmware_image,
    validate_repository_budget,
)


# Arduino-ESP32 2.0.17 adds -fno-lto to the final link even when project
# sources are compiled with -flto. This post-framework script makes the release
# policy consistent at both stages. The verbose build is retained as evidence.
link_flags = [
    flag for flag in env.get("LINKFLAGS", []) if str(flag) != "-fno-lto"  # type: ignore[name-defined]
]
env.Replace(LINKFLAGS=link_flags)  # type: ignore[name-defined]
env.AppendUnique(LINKFLAGS=["-flto"])  # type: ignore[name-defined]


def enforce_image_budget(source, target, env) -> None:  # noqa: ANN001
    del source, env
    image_path = Path(target[0].get_abspath())
    try:
        _, layout = validate_repository_budget()
        image_bytes, margin_bytes = validate_firmware_image(
            image_path, layout.firmware_image_max_bytes
        )
    except BudgetValidationError as exc:
        raise RuntimeError(f"firmware image budget failed: {exc}") from exc
    print(
        "Firmware image budget: PASS "
        f"({image_bytes} bytes, {margin_bytes} bytes margin)"
    )


env.AddPostAction(  # type: ignore[name-defined]
    "$BUILD_DIR/${PROGNAME}.bin", enforce_image_budget
)
