rehuff
======

| Vorbis stream compressor, originally released here:
  http://lists.xiph.org/pipermail/vorbis-dev/2002-July/006253.html
| The following fixes have been applied:
| http://lists.xiph.org/pipermail/vorbis-dev/2002-August/006287.html
| http://lists.xiph.org/pipermail/vorbis-dev/2002-August/006295.html

Seeking issue
=============

You may get the following messages when attempting to seek a file
created by this program:

::

   Negative granulepos on vorbis stream outside of headers. This file was created by a buggy encoder  

::

   [ogg @ ...] Page at ... is missing granule

This issue can be fixed by remuxing the Ogg file using ffmpeg:

::

   ffmpeg -i /path/to/broken/file.ogg -acodec copy /path/to/fixed/file.ogg

Non-stereo file issue
=====================

| Non-stereo files are not suported by this program.
| There is no support for “floor0 and res0 and res1” in this program.

More information
================

Check the following webpage for more information about rehuff:
https://wiki.xiph.org/index.php?title=Rehuff

License
=======

Non-free license. Check ``README_orig`` for more details.
