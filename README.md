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

	brew install autoconf ffmpeg freetype gettext ffms2 fftw fribidi libass m4
	brew install --devel --with-gc64 luajit
	brew install --HEAD icu4c
	brew link --force icu4c
	brew link --force gettext
	brew install --HEAD --c++11 --with-icu4c boost

wxWidgets is located in vendor/wxWidgets, and can be built like so:

	CPPFLAGS="$CPPFLAGS -D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=1" \
	./configure --disable-aboutdlg --disable-animatectrl --disable-aui --disable-any \
	--disable-bannerwindow --disable-base64 --disable-calendar --disable-caret \
	--disable-cmdline --disable-colourpicker --disable-compat28 --disable-config \
	--disable-constraints --disable-datepick --disable-dctransform --disable-debugreport \
	--disable-dialupman --disable-docview --disable-filehistory --disable-finddlg \
	--disable-fs_archive --disable-fs_inet --disable-fs_zip --disable-fsvolume \
	--disable-fswatcher --disable-gif --disable-help --disable-html --disable-ipc \
	--disable-joystick --disable-jpeg --disable-largefile --disable-markup --disable-mdi \
	--disable-mediactrl --disable-metafiles --disable-miniframe --disable-notifmsg \
	--disable-numberdlg --disable-pcx --disable-pnm --disable-postscript \
	--disable-prefseditor --disable-printarch --disable-progressdlg --disable-propgrid \
	--disable-protocol --disable-protocols --disable-rearrangectrl --disable-ribbon \
	--disable-richtext --disable-richtooltip --disable-snglinst --disable-sockets \
	--disable-sockets --disable-sound --disable-splash --disable-splines \
	--disable-std_iostreams --disable-svg --disable-tarstream --disable-tiff \
	--disable-tipdlg --disable-tipwindow --disable-url --disable-webkit --disable-webview \
	--disable-wizarddlg --disable-xrc \
	--enable-geometry --enable-imaglist --enable-listctrl --enable-stc --with-cocoa \
	--with-libpng=yes --with-macosx-version-min=10.9 \
	--with-opengl \
	--without-libjpeg --without-libtiff --without-regex \
	&& make

Once the dependencies are installed, build Aegisub with `autoreconf && ./configure --with-wxdir=/path/to/Aegisub/vendor/wxWidgets && make && make osx-bundle`.
`autoreconf` should be skipped if you are building from a source tarball rather than `git`.

### Debian/Ubuntu
1. Thank @wangqr in issue [#121](https://github.com/Aegisub/Aegisub/issues/121#issuecomment-500085028). Upgrading the wxGTK3 to 3.1.3, by following [this guide](https://wiki.codelite.org/pmwiki.php/Main/WxWidgets31Binaries) which is linked from the [wxWidgets download page](https://www.wxwidgets.org/downloads/).
2. Dependency.
```
sudo apt install -y \
  autoconf libtool automake \
  libfreetype6-dev libfontconfig1-dev libass-dev \
  libboost-dev libboost-chrono-dev libboost-filesystem-dev \
  libboost-locale-dev libboost-regex-dev libboost-thread-dev \
  libffms2-dev libuchardet-dev libfftw3-dev \
  libice-dev \
  luajit ffmpeg intltool
```
3. `autoreconf && ./configure && make -j 4 && sudo make install`

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
