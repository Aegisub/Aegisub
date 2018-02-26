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

	brew install boost --c++11 --with-icu
	brew install autoconf ffmpeg fontconfig freetype2 fftw3 fribidi libass wxmac

[ffms2](http://github.com/FFMS/ffms2) currently does not have a homebrew formula, but with ffmpeg installed should be a simple `./configure && make && make install` to install.

Once the dependencies are installed, build Aegisub with `autoreconf && ./configure && make && make osx-bundle`.
`autoreconf` should be skipped if you are building from a source tarball rather than `git`.

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
