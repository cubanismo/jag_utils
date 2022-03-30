Jaguar SDK Utilities
====================

This is a dumping ground for the source code of various small Jaguar development
utilities. Most of these were included as binaries in the original Jaguar DOS,
Linux, and/or ST SDKs, but there are a few additional ones as well. The basis
for size, allsyms, symval, and filefix is source code I found while digging
through the Jaguar Development Sources dump that I retrieved from here:

http://www.3do.cdinteractive.co.uk/viewtopic.php?f=35&t=3430

From the "Tool sources" link. Unzip it, look in the "SIZE" directory.

Included Programs
-----------------

* **size**: Same as size.exe from the original Jaguar DOS SDK files. Similar in
purpose to the GNU utility of the same name, it can be used to print the
section sizes and symbols from a DRI/Alcyon or BSD/COFF format ABS executable or
object file.

* **allsyms**: Print all the symbols and their values given the same types of
files as **size**.

* **symval**: Print the value of the specified symbol from the same types of
files as **size**.

* **filefix**: Same as filefix.exe from the original Jaguar DOS SDK files.
Extracts the text, data, and symbol sections from a DRI/Alcyon or BSD/COFF
format ABS executable, outputing them as individual files or a headerless ROM
image.
