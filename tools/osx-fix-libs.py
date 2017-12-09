#!/usr/bin/env python

import re
import sys
import os
import shutil
import stat

is_bad_lib = re.compile(r'(/usr/local|/opt)').match
is_sys_lib = re.compile(r'(/usr|/System)').match
otool_libname_extract = re.compile(r'\s+(/.*?)[\(\s:]').search
goodlist = []
badlist = []
link_map = {}

def otool(cmdline):
	pipe = os.popen("otool " + cmdline, 'r')
	output = pipe.readlines()
	pipe.close()
	return output

def collectlibs(lib, masterlist, targetdir):
	global goodlist, link_map
	liblist = otool("-L '" + lib + "'")
	locallist = []

	for l in liblist:
		lr = otool_libname_extract(l)
		if not lr: continue
		l = lr.group(1)
		if is_bad_lib(l) and not l in badlist:
			badlist.append(l)
		if ((not is_sys_lib(l)) or is_bad_lib(l)) and not l in masterlist:
			locallist.append(l)
			print "found %s:" % l

			check = l
			link_list = []
			while check:
				if os.path.isfile(check) and not os.path.islink(check):
					os.system("rsync --chmod=u=rwX,go=rX --no-owner --no-group '%s' '%s'" % (check, targetdir))
					print "    FILE %s ... copied to target" % check
					if link_list:
						for link in link_list:
							link_map[link] = os.path.basename(check)
					break
					
				if os.path.islink(check):
					print "    LINK %s" % check
					link_list.append(os.path.basename(check))
					check = os.path.dirname(check) + "/" + os.readlink(check)

		elif not l in goodlist and not l in masterlist:
			goodlist.append(l)
	masterlist.extend(locallist)

	for l in locallist:
		collectlibs(l, masterlist, targetdir)

exit;
binname = sys.argv[1]
targetdir = os.path.dirname(binname)
print "Searching for libraries in ", binname, "..."
libs = [binname]
collectlibs(sys.argv[1], libs, targetdir)


print
print "System libraries used..."
goodlist.sort()
for l in goodlist:
	print l


print
print "Fixing library install names..."
in_tool_cmdline = "install_name_tool "
for lib in libs:
	libbase = os.path.basename(lib)
	if libbase in link_map:
		libbase = link_map[libbase]
	in_tool_cmdline = in_tool_cmdline + ("-change '%s' '@executable_path/%s' " % (lib, libbase))
for lib in libs:
	libbase = os.path.basename(lib)

	if libbase in link_map:
		libbase = link_map[libbase]
		print "%s -> @executable_path/%s (REMAPPED)" % (lib, libbase)
	else:
		print "%s -> @executable_path/%s" % (lib, libbase)

	os.system("%s -id '@executable_path/%s' '%s/%s'" % (in_tool_cmdline, libbase, targetdir, libbase))
	sys.stdout.flush()

if badlist:
	print
	print "WARNING: The following libraries have blacklisted paths:"
	for lib in sorted(badlist):
		print lib
	print "These paths normally have files from a package manager, which means that end result may not work if copied to another machine."

print
print "All done!"
