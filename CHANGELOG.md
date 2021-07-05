# Changelog

## Changes in release 2.8.20

Fix missing initialization of `inTerminator` and `outTerminator`.
Thanks to Krisztián Löki.

New checksums `%<lrc>` and `%<hexlrc>`.
Thanks to Marcio Paduan Donadio.

Make timestamp output in error and debug messages optional with new
iocsh variable `streamMsgTimeStamped`. Default is 1.

Reduce number of duplicate error messages. Dead time for repeated messages
is configurable with new iocsh variable `streamErrorDeadTime`.
Thanks to Dominic Oram.

Shifted some noisy debug messages to `streamDebug` level 2.

In `STREAM_PROTOCOL_PATH`, allow both `:` and `;` separators on all
operating systems to make startup scripts OS independent.
On Windows however, an initial single letter followed by a `:` is detected
as a drive letter. Thus single letter directories must be separated with
`;` or followed by `\` or `/` on Windows.

Allow absolute path protocol files. Those will not use
`STREAM_PROTOCOL_PATH`. 
Changed the default search path from `"."` to `NULL`.

Only poll for `I/O Intr` records while `interruptAccept` is true,
in particular not between `iocPause` and `iocRun`.

Added this change log and a document about contributing to StreamDevice.

## Changes in release 2.8.19

Make colorization of error and debug messages optional with new
iocsh variable `streamDebugColored`. Defaults to 1 when output is a tty
and 0 otherwise.

## Changes in release 2.8.18

Another format string fix for VxWorks in checksum converter.

## Changes in release 2.8.17

Fix format strings for VxWorks 6.9 in checksum converter.

Fix some compiler warnings on Windows.

Fix some missing header files installations.

## Changes in release 2.8.16

Drop support of upper level `configure/` directory for improved
compatibility with SynApps.

Fix problem with extra spaces after protocol name in record links.

Make version string generation more robust.

## Changes in release 2.8.15

Do not overwrite `HOST_OPT` flag. 

Fix version string generation for zip downloads.

On Windows use `;` and `\` in `STREAM_PROTOCOL_PATH`.
(Was not the case before due to typo in macro name.)

Link streamApp with `seq` and `pv` libraries when `SNCSEQ` is defined in
RELEASE file.

## Changes in release 2.8.14

Fix version string generation from git tags.

## Changes in release 2.8.13

Fix bug with integer datatype handling in redirectons.

## Changes in release 2.8.12

Apply GPL-3.0.

## Changes in release 2.8.11

Fix buffer overrun.
Thanks to Garth Brown and Bruce Hill.

Fix bug in 64 bit data type handling in array records (`aai`, `aao`,
`waveform`).
Thanks to Andrew Johnson.

Fix Windows linker problem.
Thanks to Freddie Akeroyd.

Allow `(,)` inside protocol parameters. Limitation: Parenthesis must be in
matching pairs. Commas within inner parenthesis are consideres part of the
argument. Example: `protocol(arg1, (arg2, still arg 2), arg3)`.

New checksums `%<brksCryo>`, `%<xor8ff>`, `%<nsum8>`, `%<nsum16>`,
`%<nsum32>`, `%<notsum>`.
Thanks to Mark Davis.

Some error message cleanup.

## Changes in release 2.8.10

Fixes for Windows / Visual Studio 2010.
Thanks to Freddie Akeroyd.

Fix infinite loop in `I/O Intr` scanned records.

Enable error messages by default and always show error messages too when
debug messages are enabled.

Make sure any console output from StreamDevice can be redirected.

Fix some problems when building for old EPICS R3.13.

## Changes in release 2.8.9

Leave `asynTraceMask` alone. Before, streamDevice had set `setTraceMask`
to 0 initially.

Link with `sscan` module if `SSCAN` is defined in RELEASE file because the
`calc` module may need it.

Fix in regsub converter: Avoid infinite loop if an empty string is matched.
Add support for toupper/tolower to regsub converter.

Fix build problem observed on MacOS related to clash between `wait()`
system function and the StreamDevice `wait` command.

Allow `%%` in addition to `\%` to escape `%` in protocols (compatibility
with `printf`/`scanf`).

Have separate dbd file for applications without scalcout support built in.

Improved compatibility with SynApps. (fix of the attempt in 2.8.6)

## Changes in release 2.8.8

Fix problem with `in` command hanging forever if it is the the first
command of the protocol and the device is offline.

## Changes in release 2.8.7

Enable colored error/debug output on Windows.

Changed error/debug hex output highlight from fat to inverse.

Fix build problem on Windows.

## Changes in release 2.8.6

Improved compatibility with SynApps.

Fix redirection to records with names not starting with a letter or
underscore.

### Fixes/improvements for `mbbo` device support:

Use `.MASK` and `.SHFT` fields even if no `xxVL` field is defined
(and thus `.VAL` is used instead of `.RVAL`).

Bugfix: Set `mbbo` to unknown state `0xffff` if in readback no state
matches.

Bugfix: Use defined state alarms when `mbbo` is updated by `@init` handler.

## Changes in release 2.8.5

A `"\?"` at the end of an input format now matches an empty string.
Used to do so unintendedly in earlier versions but had been changed.

## Changes in release 2.8.4

Bugfix: After `@init` had been triggered with "magic value" 2 in `.PROC`
field, reset the field to 0.

## Changes in release 2.8.3

Increase limits on filename, protocolname, busname in record links.

Bugfix: Fix typo calcout device support that prevented compiling it.

## Changes in release 2.8.2

Bugfix: Fix string termination reading into char array records (`waveform`,
`aai`, `aao`, `lsi`, `lso`).

## Changes in release 2.8.1

Allow empty parameter list `()` in record links.

## Changes between 2.7 and 2.8.0

### Build system

Drop support for (buggy) cygnus-2.7.2 gcc (as used in some old cygwin
version).

Add standard EPICS App build system (`configure/`).

Some re-ordering of directories.
When upgrading from 2.7 or earlier to 2.8, better start with a fresh
source directory to avoid problems with files that have moved.

### New Features

Support for 64 bit integers. This affects some conversions (e.g. in `ai`
and `ao` records).

Device support for new record types `int64in`, `int64out`, `lsi` and `lso`
added.

Support for new `INT64` and `UINT64` types in `aai`, `aao` and `waveform`
records.

Allow spaces in protocol parameter list in record link.
One space after the opening `(`, before and after the separating `,` and
before the closing `)` ignored. More spaces are part of the parameter.

String format `%s` can now pad with 0-bytes instead of spaces using the
`0` flag.

New checksums `%<cpi>` and `%<leybold>`.

Properly distinguish beween signed and unsigned integers, e.g. when
converting to double.
Allow signed enums to do this: `%#{backwards=-1|stop=0|forwards=1}`.

#### New connection handling

Run `@init` handler each time the device (re-)connects
(as long as asynDriver detects connection changes), also at each `iocRun`
(after being paused with `iocPause`), and after each `streamReload`.

Also run `@init` of a record when the "magic value" 2 is written to the
`.PROC` field or when new `streamReinitRecord` shell function is called.

Trigger monitors when `@init` updates output records (after the intial run
in `iocInit`).

Use `"COMM"` error code in `.STAT` when device is disconnected.

If write fails because device has disconnected, try to re-connect and re-do
write once.

### Bug fixes 

Fix parser bug when command reference was followed by `}` without `;`.

Fix potential buffer overrun during `streamReload`.
After `streamReload`, properly delete handlers and variables that do not
exist any longer.

Fix for waveforms of unsigned integers.

Several Windows related fixes and support building shared libraries on
Windows.

Fix C++11 warnings.

Check formatting of `exec` command for errors.

Fix bug in `dbior` output.

Fix BCD `%D` format and allow negative values.

Fix signed hex and octal formats.

Fix in regsub: Do not run regsub again on substituted string.

### Shell functions

New functions `streamReportRecord` and `streamSetLogfile`.

Support output redirection to file of stream shell functions
and protocol dump via `dbior`.

### Error and debug output

Cleanup some debug and error messages.

Hex bytes in debug/error output are now highlighted.

`StreamError` can now be set before `iocInit`.

Avoid repeating error messages.
Thanks to Ben Franksen.

### Documentation

HTML files converted to HTML 5. 

Update and improve several chapters.

## Changes in releases up to 2.7.14

Not available. See git log.
