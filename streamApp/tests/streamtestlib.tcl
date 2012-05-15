#
# Usage
# 1) source this file
# 2) define variables records, protocol, starup
# 3) call startioc
# 4) use ioccmd, assure, receive, send,...
# 5) call finish

set testname [file tail $argv0]

proc bgerror msg {
    error $::errorInfo
}

set debug 0
proc debugmsg {string} {
    global debug
    if $debug {puts $string}
}

proc deviceconnect {s addr port} {
    debugmsg "incoming connenction"
    global sock
    set sock $s
    fconfigure $sock -blocking no -buffering none -translation binary 
    fileevent $sock readable "receiveHandler $sock"
}

set inputbuffer {}
proc receiveHandler {sock} {
    global inputbuffer inputlog
    set input [read $sock]
    puts -nonewline $inputlog $input
    append inputbuffer $input
    debugmsg "receiving \"[escape $inputbuffer]\""
    if [eof $sock] {
        close $sock
        debugmsg "connection closed by ioc"
        return
    }
}

proc startioc {} {
    global debug records protocol startup port sock ioc testname env streamversion
    set fd [open test.db w]
    puts $fd $records
    close $fd
    set fd [open test.proto w]
    puts $fd $protocol
    close $fd
    set fd [open test.cmd w 0777]
    
    if [info exists streamversion] {
        puts $fd "#!/usr/local/bin/iocsh"
        puts $fd "require stream2,$streamversion"
    } else {
        puts $fd "#!../O.$env(EPICS_HOST_ARCH)/streamApp"
        puts $fd "dbLoadDatabase ../O.Common/streamApp.dbd"
        puts $fd "streamApp_registerRecordDeviceDriver"
    }
    puts $fd "epicsEnvSet STREAM_PROTOCOL_PATH ."
    puts $fd "drvAsynIPPortConfigure device localhost:$port"
    puts $fd "dbLoadRecords test.db"
    puts $fd $startup
    puts $fd "iocInit"
    puts $fd "dbl"
    puts $fd "dbior stream 2"
    puts $fd "var streamDebug 1"
    close $fd
    if [info exists streamversion] {
        set ioc [open "|iocsh test.cmd >& $testname.ioclog 2>@stderr" w]
    } else {
        set ioc [open "|../O.$env(EPICS_HOST_ARCH)/streamApp test.cmd >& $testname.ioclog 2>@stderr" w]
    }
    fconfigure $ioc -blocking yes -buffering none
    debugmsg "waiting to connect"
    vwait sock
}

set lastcommand ""
set line 0
proc ioccmd {command} {
    global ioc
    global lastcommand
    global line
    set lastcommand $command
    set line 0
    debugmsg "$command"
    puts $ioc $command
}

proc send {string} {
    global sock lastsent
    set lastsent $string
    puts -nonewline $sock $string
    flush $sock
}

set timeout 5000
proc receive {} {
    global inputbuffer timeoutid timeout
    set timeoutid [after $timeout {
        set inputbuffer {}
    }]
    if {$inputbuffer == {}} { vwait inputbuffer }
    after cancel $timeoutid
    if {$inputbuffer == {}} {
        return -code error "Error in receive: timeout"
    }
    set index [string first "\n" $inputbuffer]
    if {$index > -1} {
        set input [string range $inputbuffer 0 $index]
        set inputbuffer [string range $inputbuffer [expr $index+1] end]
    } else {
        set input $inputbuffer
        set inputbuffer {}
    }
    return $input
}

set faults 0
proc assure {args} {
    global faults
    global lastcommand
    global lastsent
    global line
    
    incr line
    set input {}
    for {set i 0} {$i < [llength $args]} {incr i} {
        if [catch {lappend input [receive]} msg] {
            puts stderr $msg
            break
        }
    }
    set notfound {}
    foreach expected $args {
        set index [lsearch -exact $input $expected]
        if {$index > -1} {
            set input [lreplace $input $index $index]
        } else {
            lappend notfound $expected
        }
    }
    if {[llength $notfound] || [llength $input]} {
        puts stderr "In command \"$lastcommand\""
        if [info exists lastsent] {
            puts stderr "last sent: \"[escape $lastsent]\""
        }
    }
    foreach string $notfound {
        puts stderr "Error in assure: line $line missing \"[escape $string]\""
    }
    foreach string $input {
        puts stderr "Error in assure: got unexpected \"[escape $string]\""
    }
    if {[llength $notfound] || [llength $input]} {incr faults}
}

proc escape {string} {
    set result ""
    set length [string length $string]
    for {set i 0} {$i < $length} {incr i} {
        set c [string index $string $i]
        scan $c %c n
        if {$n == 13} {
            append result "\\r"
        } elseif {$n == 10} {
            append result "\\n"
        } elseif {($n & 127) < 32} {
            append result [format "<%02x>" $n]
        } else {
            append result $c
        }
    }
    return $result
}

proc finish {} {
    global ioc timeout testname faults
    set timeout 1000
    while {![catch {set string [receive]}]} {
        puts stderr "Error in finish: unexpected \"[escape $string]\""
        incr faults
    }
    after 100
    close $ioc
    if $faults {
        puts "\033\[31;7mTest failed.\033\[0m"
        exit 1
    }
    puts "\033\[32mTest passed.\033\[0m"
    eval file delete [glob -nocomplain test.*] StreamDebug.log $testname.ioclog
}

set port 40123
socket -server deviceconnect $port
set inputlog [open "test.inputlog" w]

# SLS style driver modules (optionally with version)
if {[lindex $argv 0] == "-sls"} {
    set streamversion [lindex $argv 1]
    set argv [lrange $argv 2 end]
}
