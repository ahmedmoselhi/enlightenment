![Enlightenment](/data/readme/enlightenment.png)
# Enlightenment

![Screenshot](/data/readme/screenshot.png)

-----

*Please report bugs/issues at*
[git.enlightenment.org](https://git.enlightenment.org/enlightenment/enlightenment/issues)

-----

Enlightenment is a Window Manager, Compositor and basic "Desktop
Shell". It replaces your environment that runs your day to day GUI
Desktop. It includes a built-in file manager, Launcher, Shelves,
Settings dialogs, Menus, Audo Mixer controls and much much more.

-----

## Requirements

Must:

* [efl](https://git.enlightenment.org/enlightenment/efl)
* libpam (Required on Linux, not BSD)

Highly suggested:

* libexif (exif metadata display support)
* bluez5 (BT support and l2ping tool)
* connman
* bc (For everything calculator)
* pulseaudio
* acpid (Unless your system doesn't have ACPI at all)
* packagekit (For packagekit module updates status)
* udisks2
* gdb (If you want automatic bactraces in ~/.e-crashdump.txt)

-----

## Compiling and Installing

Meson is the build system used for this project. For more information
please see [mesonbuild.com](https://mesonbuild.com)

You will need normal build tooling installed such as a compiler (gcc
or clang for example), pkg-config, ninja, any relevant package-dev or
package-devel packages if your distribution splits out development
headers (e.g. libc6-dev) etc.

Depending on where dependencies (like efl) are installed, you might have to
set your `PKG_CONFIG_PATH` environment variable like:
```
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
```

Also note that some distributions like to add extra arch directories
to your library locations so you might have to have more like:
```
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig
```

You will need to enusre that the destination library directory (e.g.
`/usr/local/lib` is in your /etc/ld.so.conf or /etc/ld.so.conf.d/
files and after insgtaling anything that installes libraries you
re-run `ldconfig`. Please see relevant documentation on ldconfig and
ld.so.conf for your distribution.

You might also want to add the destination bin dir to your environment
variable PATH (see documentation on your shell PATH variable) such as:
```
export PATH=/usr/local/bin:/usr/bin:/bin
```

Normal compilation in /usr/local:
```
meson . build
ninja -C build
sudo ninja -C build install
```

For meson build generic options:
```
meson --help
```

For a list of project specific options supported:
```
cat meson_options.txt
```

To set 1 or more project specific options:
```
meson --prefix=/path/to -Doption=value [-Dother=value2] [...] . build
```

To display current configuration:
```
meson configure build
```

The above will only work after at least the following is done:
```
meson . build
```

### Quick build help

How clean out the build and config and start fresh:
```
rm -rf build
```

How to make a dist tarball and check its build:
(must do it from git tree clone and commit all changes to git first)
```
ninja -C build dist
```

How to change prefix:
```
meson --prefix=/path/to/prefix . build
```

How to install in a specific destination directory for packaging:
```
DESTDIR=/path/to/destdir ninja -C build install
```

How to build with verbose output (full commands run):
```
ninja -C build -v
```
-----

**NOTE:** If you do not want security issues make sure sysactions.conf is in
/etc/enlightenment (not PREFIX/etc/enlightenment) as this is the first place
it looks at. This file is intended to be customized by packagers and
system integrators to match your policies and scripts/tools.

**NOTE:** To enable wayland support (still considered experimental and not for
regular end users) use the meson -Dwl=true option. To run enlightenment in
wayland mode, just log on on any VT and run enlightenment_start. If you
wish to debug and see all the output try using something like screen then
attaching to the remote screen session by sshing in etc.


