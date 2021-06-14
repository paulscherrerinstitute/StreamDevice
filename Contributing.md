# Contributing to StreamDevice

Contributions from the EPICS community (and others) are welcome.
To ease the integration process, please follow the following guidelines.

All contributions should be done as a pull request to the git repository
https://github.com/paulscherrerinstitute/StreamDevice. Make sure to provide
meaningful commit messages (no essay but more than "changes").

For small modifications, a patch file is sufficient. Send it to me or better
create an issue on https://github.com/paulscherrerinstitute/StreamDevice/issues
and attach the patch file.

Justify your change requests. Write a short summary for your pull request
to explain what the change is about and what it improves or which bug it
fixes. Use the issue system on github to report bugs.

I reserve the right to accept or reject contributions or to request
modifications before I accept them.

Of course, you may as well report bugs without providing a solution yourself.

## Code compatibility

All code must compile for any EPICS release from at least R3.14.12 up to the
latest one. Likewise the code must be compatible with any operating system
supported by EPICS, like Linux, Windows, MacOS, RTEMS and VxWorks.
In particular VxWorks 5 compatibility rules out many modern C++ features.
But there are also other platforms that for example do not support C++11, so
don't use it. 
There should also be no compiler warning on any OS.

Avoid compiler dependent features like #pragmas, assumptions on byte order,
type size (in particular the size of pointers and long int) and other
non-portable things like the availability of certain header files. Make sure
non-portable code parts are enclosed in proper compiler branches and provide
working implementations for all architectures.

Make sure that the code in AsynDriverInterface stays compatible with old
and new versions of asyn driver.

The core of StreamDevice does not depend on EPICS. This is on purpose, to be
able to use it in other control system frameworks. Modifications should not
add EPICS dependencies except to StreamEpics and AsynDriverInterface, or the
new dependency must be in a separate file which can be left out of the build
without jeopardizing the main functionality of StreamDevice.

The code must not depend on external libraries that may not be available on
all systems, except if provided as a separate file which can be left out of
the build on platforms that do not support the library.

## Language

Write in English. That includes all identifiers (variables, functions, ...),
comments, documentation and commit messages. Check your spelling.

## File formats

All files are in Unix format (\n line terminators). Do not change them to
any other format (e.g. Windows with \r\n terminators). Do not add new files in
other formats.

The files must contain only ASCII characters. Do not use any Unicode multi-byte
characters (including byte order marks) or any pre-Unicode code page dependent
characters (e.g. umlaut), not even in comments. Do not use form feed, vertical
tab, or other control characters except newline.

Indents are 4 spaces. Do not use tabs (except in Makefiles). Make sure your
editor is set up accordingly.

Files must end in a newline and there must be no spaces at the end of lines.
Do not add excessive amount of newlines at the end of files.

Do not add any editor configurations (e.g. for emacs) to the files. Also do
not add any configuration files or directories for development environments,
editors, etc.

-------

Dirk Zimoch <dirk.zimoch@psi.ch>, June 2021

