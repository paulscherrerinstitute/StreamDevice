# StreamDevice

_StreamDevice_ is a generic [EPICS](https://epics.anl.gov/)
device support for devices with a "byte stream" based
communication interface.
That means devices that can be controlled by sending and receiving
strings (in the broadest sense, including non-printable characters
and even null-bytes). 
Examples for this type of communication interface are
serial line (RS-232, RS-485, ...), 
IEEE-488 (also known as GPIB or HP-IB), and telnet-like TCP/IP.

_StreamDevice_ is not limited to a specific device type or manufacturer
nor is it necessary to re-compile anything to support a new device type.
Instead, it can be configured for any device type with _protocol files_
in plain ASCII text which describes the commands a device understands
and the replies it sends.

For a full documentation see
https://paulscherrerinstitute.github.io/StreamDevice.
