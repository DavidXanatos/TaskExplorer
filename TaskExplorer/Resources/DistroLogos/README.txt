DistroLogos
===========

Distribution logos shown in the System Information panel on Linux.

These are loose PNG files rather than compiled-in Qt resources on purpose:

  * a logo can be added or corrected for a newly released distribution by
    dropping a file in here, with no rebuild and no new release of
    TaskExplorer;

  * distribution logos are trademarks of their respective projects, and
    keeping them out of the binary keeps that a packaging decision rather
    than a source-tree one.

At runtime this folder is expected next to the TaskExplorer executable.


Naming
------

Files are named after the ID field of /etc/os-release, lowercased, with a
.png extension. TaskExplorer looks for them in this order and uses the first
that exists:

  1.  <ID>-<VERSION_ID>.png      ubuntu-26.04.png
        Use only when a specific release needs its own artwork.

  2.  <ID>.png                   ubuntu.png  debian.png  fedora.png
        The normal case.

  3.  <ID_LIKE entry>.png        debian.png, for a Debian derivative that
        has no logo of its own. ID_LIKE may list several parents; they are
        tried in the order the distribution declares them.

  4.  linux.png
        Generic fallback for anything unrecognised.

To see what a given machine reports:

    grep -E '^(ID|ID_LIKE|VERSION_ID|LOGO)=' /etc/os-release


If nothing here matches
-----------------------

TaskExplorer falls back to the logo the distribution itself installed - the
LOGO key from os-release resolved against the icon theme, then
/usr/share/pixmaps/<LOGO>.png, then /usr/share/pixmaps/<ID>-logo.png.

If that also fails, no icon is shown. This is not an error.


Image format
------------

PNG, square, with transparency. The panel scales to 64x64 preserving aspect
ratio, so 128x128 or 256x256 sources look best on HiDPI displays. Larger than
256x256 is wasted.
