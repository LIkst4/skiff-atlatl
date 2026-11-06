#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# (custom) skiff-atlatl's installer page for its GitHub Pages site, from Skiff's web/index_pkg.html: the same installer
# (fm1pkg.js, fm1ota.js, fm1backup.js, conv.js inlined, as web/make_site.py does) with the fork's package, name and
# links. The site's layout is flat (installer/, editor/, firmware/), not make_site.py's webapp/installer/.
#   web/make_installer.py build/felucca.fwsc "2.2 · atlatl-dev" site
#     -> site/installer/index.html, site/firmware/skiff-atlatl.fwsc (+ LICENSE, LICENSING.md, LICENSES/)
import html as H
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from make_site import product_of, strip_module  # noqa: E402

REPO = "https://github.com/LIkst4/skiff-atlatl"
NAME = "skiff-atlatl.fwsc"


def main(pkg, version, out):
    out = Path(out)
    raw = Path(pkg).read_bytes()
    product = product_of(raw)
    if not product.startswith("FM-1_742"):
        raise SystemExit(f"{pkg}: identity {product!r} is not skiff-atlatl's (FM-1_742xx)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no Felucca loader in it (an official package patched? the site ships ./build.sh's)")
    page = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((HERE / f).read_text(encoding="utf-8"))
                    for f in ("fm1pkg.js", "fm1ota.js", "fm1backup.js", "conv/conv.js"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + NAME,
                       "packages": [{"product": product, "pkg": "../firmware/" + NAME}]})
    swaps = [
        ("/*LIB*/", lib), ("/*META*/", meta),
        ("<title>Skiff</title>", "<title>skiff-atlatl installer</title>"),
        ("<h1>Skiff</h1>", "<h1>skiff-atlatl</h1>\n  <p class=\"small\">A fork of Skiff for the FM-1 (" + H.escape(version)
         + "). This page installs skiff-atlatl, not the original Skiff.</p>"),
        ("../../firmware/", "../firmware/"),
        ('<p><a href="../../learn/" style="color:inherit" data-t="learn"></a></p>',
         f'<p><a href="{REPO}/blob/atlatl-dev/docs/PERFORMANCE.md" style="color:inherit">Performance guide</a> · '
         f'<a href="{REPO}/blob/atlatl-dev/docs/PERFORMANCE.es.md" style="color:inherit">Guía de performance</a></p>'),
        ('<p class="small">Skiff by illuminus5,', '<p class="small">skiff-atlatl by LIkst4, with: OberVave\'s factory '
         'programs and X-MOD / DRIFT ideas (zvenson, GPL-3.0); the screen mirror from fm1-mirror (Patrick F. Kellogg, MIT); '
         'FMini-1\'s Minimoog voice (Guilherme Ruppert, GPL-3.0); the 8-channel USB audio after Kitten Synths\' fix-17 '
         '(Black Kitten Devices); X0X\'s TB-303 oscillator and filter (Charles Vestal, GPL-3.0; after Open303 by Robin Schmidt, MIT); '
         'Hortator\'s drum models (DEADACTIVE, GPL-3.0; its Plaits drum ports MIT, Emilie Gillet); models after Mutable '
         'Instruments\' Plaits and Rings (Emilie Gillet, MIT); POLY MOD laid out as Melodee\'s Prophet panel (Kerem Kilic); '
         'INSERT types after Airwindows (Chris Johnson, MIT).<br>A fork of Skiff by illuminus5,'),
        ('<a href="https://github.com/illuminus5/skiff" target="_blank" rel="noopener" style="color:inherit">github.com/illuminus5/skiff</a>',
         f'<a href="{REPO}" target="_blank" rel="noopener" style="color:inherit">github.com/LIkst4/skiff-atlatl</a>'),
        ("M-VAVE, FM-1 and SMK-37 are trademarks of their respective owners.<br>",
         "M-VAVE, FM-1 and SMK-37 are trademarks of their respective owners; other product names in the guides only "
         "describe what a feature was modelled on. skiff-atlatl is not affiliated with or endorsed by any of them.<br>"),
    ]
    for old, new in swaps:
        if old not in page:
            raise SystemExit(f"index_pkg.html: {old[:60]!r} not found; update make_installer.py")
        page = page.replace(old, new)
    if "illuminus5/skiff\"" in page:
        raise SystemExit("index_pkg.html: a link to Skiff's repository is left")
    inst, fw = out / "installer", out / "firmware"
    for d in (inst, fw / "LICENSES"):
        d.mkdir(parents=True, exist_ok=True)
    (inst / "index.html").write_text(page, encoding="utf-8")
    (fw / NAME).write_bytes(raw)
    names = sorted(f.name for f in (HERE.parent / "LICENSES").glob("*.txt"))
    for n in names:
        shutil.copy(HERE.parent / "LICENSES" / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>skiff-atlatl licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    print(f"installer: {inst / 'index.html'} ({len(page)} B), firmware/{NAME} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__ or "usage: make_installer.py PKG VERSION OUT")
    main(*sys.argv[1:])
