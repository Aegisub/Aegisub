# Aegisub

For binaries and general information [see the homepage](http://www.aegisub.org).

The bug tracker can be found at http://devel.aegisub.org.

Support is available on [the forums](http://forum.aegisub.org) or [on IRC](irc://irc.rizon.net/aegisub).

## Building Aegisub

### Windows

Prerequisites:

1. Visual Studio 2015 (the free Community edition is good enough)
2. The June 2010 DirectX SDK (the final release before DirectSound was dropped)
3. [Yasm](http://yasm.tortall.net/) installed to somewhere on your path.

There are a few optional dependencies:

1. msgfmt, to build the translations
2. WinRAR, to build the portable installer
3. InnoSetup, to build the regular installer

All other dependencies are either stored in the repository or are included as submodules.

Building:

1. Clone Aegisub's repository recursively to fetch it and all submodules: `git clone --recursive git@github.com:Aegisub/Aegisub.git` This will take quite a while and requires about 2.5 GB of disk space.
2. Open Aegisub.sln
3. Build the BuildTasks project.
4. Build the entire solution.

You should now have a `bin` directory in your Aegisub directory which contains `aegisub32d.exe`, along with a pile of other files.

The Aegisub installer includes some files not built as part of Aegisub (such as Avisynth and VSFilter), so for a fully functional copy of Aegisub you now need to copy all of the files from an installed copy of Aegisub into your `bin` directory (and don't overwrite any of the files already there).
You'll also either need to copy the `automation` directory into the `bin` directory, or edit your automation search paths to include the `automation` directory in the source tree.

After building the solution once, you'll want to switch to the Debug-MinDep configuration, which skips checking if the dependencies are out of date, as that takes a while.

### OS X

A vaguely recent version of Xcode and the corresponding command-line tools are required.
Nothing older than Xcode 5 has been tested recently, but it is likely that some later versions of Xcode 4 are good enough.

For personal usage, you can use homebrew to install almost all of Aegisub's dependencies:

	brew install autoconf automake ffmpeg ffms2 fftw freetype fribidi gettext icu4c libass m4 pkg-config boost
	brew install luajit --HEAD
	brew link --force gettext
	export LDFLAGS="-L$(brew --prefix)/lib -L$(brew --prefix)/opt/icu4c/lib $LDFLAGS"
	export CPPFLAGS="-I$(brew --prefix)/include -I$(brew --prefix)/opt/icu4c/include $CPPFLAGS"
	export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix)/opt/icu4c/lib/pkgconfig:$PKG_CONFIG_PATH"

wxWidgets is located in vendor/wxWidgets, and can be built like so:

	CPPFLAGS="$CPPFLAGS -D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=0" \
	./configure --disable-all-features \
	--enable-unicode --enable-utf8 --enable-stl \
	--enable-exceptions --enable-log --enable-threads --enable-palette \
	--enable-baseevtloop --enable-selectloop --enable-gui --enable-timer --enable-menus \
	--enable-intl --enable-xlocale --enable-statusbar --enable-sysoptions \
	--enable-button --enable-bmpbutton --enable-listbook --enable-listbox --enable-listctrl \
	--enable-checkbox --enable-textctrl --enable-hyperlink --enable-treebook \
	--enable-image --enable-stopwatch --enable-scrollbar --enable-longlong \
	--enable-geometry --enable-imaglist --enable-stc --enable-hotkey --enable-regkey \
	--enable-graphics_ctx --enable-dataobj --enable-file --enable-ffile \
	--enable-streams --enable-dragimage --enable-dnd --enable-radiobox --enable-radiobtn \
	--enable-spinbtn --enable-spinctrl --enable-slider --enable-searchctrl \
	--enable-stattext --enable-statline --enable-statbox --enable-statbmp \
	--enable-toolbar --enable-tbarnative --enable-clipboard --enable-menubar \
	--enable-msgdlg --enable-filedlg --enable-fontdlg --enable-textdlg --enable-choicedlg \
	--enable-coldlg --enable-dirdlg --enable-numberdlg --enable-aboutdlg \
	--enable-dataviewctrl --enable-datepick --enable-dirpicker --enable-filectrl \
	--enable-filepicker --enable-fontpicker --enable-treectrl --enable-comboctrl \
	--enable-rearrangectrl --enable-treelist --enable-timepick --enable-accel \
	--enable-headerctrl --enable-variant --enable-datetime --enable-checklst \
	--enable-choice --enable-choicebook --enable-combobox --enable-gauge \
	--enable-tooltips --enable-validators --enable-std_string --enable-std-containers \
	--enable-popupwin --enable-controls --enable-stc --enable-calendar \
	--enable-sash --enable-splitter --enable-arcstream --enable-zipstream \
	--enable-xpm --enable-dynlib --enable-dynamicloader --enable-fontenum \
	--with-zlib --with-expat --with-libpng \
	--with-cocoa \
	--with-macosx-version-min=no \
	--with-opengl \
	--without-libjpeg --without-libtiff --without-regex \
	&& make -j10

Once the dependencies are installed, build Aegisub with `autoreconf -ivf && ./configure --with-wxdir="$PWD/vendor/wxWidgets" --with-boost="$(brew --prefix)/opt/boost" && make -j$(sysctl -n hw.ncpu) && make osx-bundle && ln -sf $PWD/src/aegisub Aegisub.app/Contents/MacOS/aegisub`.
`autoreconf` should be skipped if you are building from a source tarball rather than `git`.

After the first build, you can just run `make -j[cores]` again.

## Updating Moonscript

From within the Moonscript repository, run `bin/moon bin/splat.moon -l moonscript moonscript/ > bin/moonscript.lua`.
Open the newly created `bin/moonscript.lua`, and within it make the following changes:

1. Prepend the final line of the file, `package.preload["moonscript"]()`, with a `return`, producing `return package.preload["moonscript"]()`.
2. Within the function at `package.preload['moonscript.base']`, remove references to `moon_loader`, `insert_loader`, and `remove_loader`. This means removing their declarations, definitions, and entries in the returned table.
3. Within the function at `package.preload['moonscript']`, remove the line `_with_0.insert_loader()`.

The file is now ready for use, to be placed in `automation/include` within the Aegisub repo.

## License

All files in this repository are licensed under various GPL-compatible BSD-style licenses; see LICENCE and the individual source files for more information.
The official Windows and OS X builds are GPLv2 due to including fftw3.
