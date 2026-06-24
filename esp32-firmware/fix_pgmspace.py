"""
Pre-build script for LovyanGFX + ESP32 Arduino Core 2.0.11+ compatibility.

Patches the LovyanGFX utility/pgmspace.h to add missing macros that ESP32
Arduino Core 2.0.11+ doesn't define (pgm_read_dword_unaligned, etc).
"""
import os
Import("env")

libdeps_dir = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"])
pgmspace_path = os.path.join(
    libdeps_dir, "LovyanGFX", "src", "lgfx", "utility", "pgmspace.h"
)

if os.path.exists(pgmspace_path):
    with open(pgmspace_path, "r") as f:
        content = f.read()

    if "PGMSPACE_PATCHED" not in content:
        patch = """
/* === PGMSPACE_PATCHED === */
#if !defined(pgm_read_word_unaligned)
  #define pgm_read_word_unaligned(addr)  (*(const uint16_t *)((uintptr_t)addr))
#endif
#if !defined(pgm_read_dword_unaligned)
  #define pgm_read_dword_unaligned(addr) (*(const uint32_t *)((uintptr_t)addr))
#endif
"""
        marker = "#if !defined ( pgm_read_3byte_unaligned )"
        if marker in content:
            content = content.replace(marker, patch + "\n" + marker)
            with open(pgmspace_path, "w") as f:
                f.write(content)
            print("[OK] Patched LovyanGFX pgmspace.h macros")
else:
    print("[WARN] LovyanGFX pgmspace.h not found, skipping patch")
