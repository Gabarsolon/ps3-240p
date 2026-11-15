"""Focused offline proof for the HDMI app handshake and session controller."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler and Path("C:/Program Files/LLVM/bin/clang.exe").exists():
        compiler = "C:/Program Files/LLVM/bin/clang.exe"
    if not compiler:
        raise SystemExit("A host C compiler is required")
    out = ROOT / "build/tests"
    out.mkdir(parents=True, exist_ok=True)
    flags = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-Wno-misleading-indentation", "-D_CRT_SECURE_NO_WARNINGS",
             "-Iapp/source", "-Itests"]
    cases = [
        ("hdmi-custom", [], ["tests/test_hdmi_custom.c", "experiments/hdmi_custom/service.c",
                              "experiments/hdmi_custom/confirm.c"]),
        ("hdmi-video", ["HC_VIDEO_HOST_TEST"],
         ["tests/test_hdmi_video.c", "experiments/hdmi_custom/video.c"]),
        ("hdmi-backend", ["HC_BACKEND_HOST_TEST", "CRT_XGA_PROBE", "CRT_CUSTOM_HDMI"],
         ["tests/test_hdmi_custom_backend.c", "experiments/hdmi_custom/backend.c",
          "experiments/display_probe/service.c", "plugin/hd540_service.c"]),
        ("display-custom-native", ["DP_NATIVE_HOST_TEST", "CRT_CUSTOM_HDMI"],
         ["tests/test_display_probe_native.c", "experiments/display_probe/native.c"]),
        ("hd540-service", [], ["tests/test_hd540_service.c", "plugin/hd540_service.c"]),
        ("vsh-hd540", ["VSH_HD540_HOST_TEST", "TIMING_OFFLINE_TEST", "SYSMGR_SYNTHETIC_TEST"],
         ["tests/test_vsh_hd540.c", "plugin/vsh_hd540.c", "app/source/lv1_patch.c",
          "app/source/hd540_patch.c", "app/source/hd540_hdmi.c", "app/source/hd540_scale.c",
          "app/source/hd540_cache.c", "app/source/switch_flow.c", "app/source/sysmgr.c"]),
    ]
    for name, definitions, sources in cases:
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        subprocess.run(flags + ["-D" + define + "=1" for define in definitions] +
                       sources + ["-o", str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
