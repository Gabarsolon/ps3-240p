"""Render the production menu locally using its C renderer; no PS3 access."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "deliverables/CRT-Toolset-menu.png")
    args = parser.parse_args()
    source = (ROOT / "app/source/main.c").read_text(encoding="utf-8")
    block = re.search(r"const char \*items\[\]\s*=\s*\{(.*?)\};", source, re.S)
    assert block, "Production menu not found"
    items = [json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', block[1])]
    version = re.search(r'CRT Toolset / v([^\\"\n]+)', source)[1]
    title = f"CRT Toolset / v{version}\nPAL: candidate  NTSC: candidate"
    build = ROOT / "build/menu-preview"
    build.mkdir(parents=True, exist_ok=True)
    cfile, exe, ppm = build / "render.c", build / "render.exe", build / "menu.ppm"
    menu = ",\n".join(json.dumps(s) for s in items)
    code = r'''
#include "ui_layout.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
 if(argc!=2)return 1;
 const int width=720,height=576;
 uint32_t *pixels=calloc((size_t)width*height,sizeof(*pixels));
 if(!pixels)return 2;
 const char *items[]={ITEMS};
 ui_layout layout;ui_layout_init(&layout,width,height,0,0);
 ui_canvas canvas={pixels,width,NULL,NULL};ui_result result;ui_view view={0};
 view.title=TITLE;view.items=items;view.count=(int)(sizeof(items)/sizeof(items[0]));
 view.selected=0;view.page=-1;
 if(ui_render(&layout,&view,&canvas,&result)||!result.selection_visible)return 3;
 FILE *out=fopen(argv[1],"wb");if(!out)return 4;
 fprintf(out,"P6\n%d %d\n255\n",width,height);
 for(int i=0;i<width*height;i++){
  unsigned char rgb[3]={(unsigned char)(pixels[i]>>16),(unsigned char)(pixels[i]>>8),(unsigned char)pixels[i]};
  if(fwrite(rgb,1,3,out)!=3)return 5;
 }
 fclose(out);free(pixels);return 0;
}
'''.replace("ITEMS", menu).replace("TITLE", json.dumps(title))
    cfile.write_text(code, encoding="utf-8", newline="\n")
    compiler = shutil.which("clang") or shutil.which("gcc")
    if not compiler:
        compiler = str(Path("C:/Program Files/LLVM/bin/clang.exe"))
    subprocess.run([compiler, "-std=c11", "-O2", "-D_CRT_SECURE_NO_WARNINGS",
                    "-Iapp/source", str(cfile), "app/source/ui_layout.c", "-o", str(exe)],
                   cwd=ROOT, check=True)
    subprocess.run([str(exe), str(ppm)], check=True)
    from PIL import Image
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(ppm) as rendered:
        rendered.save(args.output)
    print(f"Local production-renderer preview: {args.output}")


if __name__ == "__main__":
    main()
