import os, sys, string, re

ws = string.whitespace

# Shorten up path function calls
dirname = os.path.dirname
basename = os.path.basename
splitext = os.path.splitext
abspath = os.path.abspath
relpath = os.path.relpath
pjoin = os.path.join

isfile = os.path.isfile
isdir = os.path.isdir
isabs = os.path.isabs
exists = os.path.exists

noext = lambda p: splitext(p)[0]
fext = lambda p: splitext(p)[-1]
reldir = lambda p,c: relpath(dirname(c), start=p)
dname = lambda p: dirname(abspath(p))
bname = lambda p: noext(basename(p))

cleanall = lambda lines: filter(
	lambda line: len(line) > 0 and not line.startswith('/'),
	map(lambda line: line.strip(ws), lines)
)

def main(args):
	if len(args) == 0:
		print 'Usage: %s file.c' % sys.argv[0]
		sys.exit(0)
	fpath = abspath(args[0])
	fasm = open(fpath, 'rt')
	lines = cleanall(fasm.readlines())
	fasm.close()
	size = 0
	sizes = { 'B': 1, 'W': 2, 'L': 4, 'Q': 8 }
	for c in map(lambda line: line[5:6], lines):
		size += sizes.get(c)
	print 'Size is: %d' % size

if __name__=='__main__':
	main(sys.argv[1:])