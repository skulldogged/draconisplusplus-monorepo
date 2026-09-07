# Python binding

In the monorepo, configure with `-Dbuild_python=true` and run `meson test -C build 'Python Binding'`.
Run Meson under the Python interpreter whose development headers/library you want to use.
On Windows, use `python -m pip install meson` followed by `python -m mesonbuild.mesonmain setup build`;
Meson's standalone executable embeds an interpreter without a development library.

Standalone wheels consume an installed `draconis-c` SDK through pkg-config:
set `PKG_CONFIG_PATH` to its `lib/pkgconfig` directory, then run `python -m pip wheel .` here.
The shared native runtime must remain available on the runtime library search path.
