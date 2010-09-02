#!/usr/bin/env wish

wm iconify .

proc createTerm {sock} {
    global socket port
    toplevel .$sock
    text .$sock.t -yscrollcommand ".$sock.v set" 
    scrollbar .$sock.v -command ".$sock.t yview"
    .$sock.t tag configure output -foreground red
    .$sock.t tag configure input -foreground darkgreen
    grid rowconfigure .$sock 0 -weight 1
    grid columnconfigure .$sock 0 -weight 1
    grid .$sock.t .$sock.v -sticky nsew
    bind .$sock.t <Destroy> "close $sock; unset socket(.$sock.t)"
    bind .$sock.t <F1> "%W delete 0.1 end"
    set socket(.$sock.t) $sock
    focus .$sock.t
    wm title .$sock "port $port <-> [fconfigure $sock -peername]" 
}

set port [lindex $argv 0]
if {$port == ""} { set port 40000 }
if [catch {
    socket -server connect $port
} msg ] {
    return -code error "$msg (port $port)"
}

proc connect {sock addr port} {
    fconfigure $sock -blocking 0 -buffering none
    createTerm $sock
    fileevent $sock readable "receiveHandler $sock"
}

proc sendReply {sock text} {
    .$sock.t mark set insert end
    .$sock.t insert end $text
    .$sock.t see end
    puts -nonewline $sock $text
}

proc checkNum {n} {
    if {[string is integer $n] && $n >= 0} {return $n}
    return -code error "argument $n must be a positive number"
}

proc receiveHandler {sock} {
    set a [read $sock]
    if [eof $sock] {
        destroy .$sock
        return
    }
    .$sock.t mark set insert end
    .$sock.t insert end $a output
    .$sock.t see end
    set l [split $a]
    if [catch {
        switch -- [lindex $l 0] {
            "disconnect" {
                sendReply $sock [string range $a 11 end]
                destroy .$sock
            }
            "echo" {
                sendReply $sock [string range $a 5 end]
            }
            "longmsg" {
                set length [checkNum [lindex $l 1]]
                sendReply $sock "[string range x[string repeat 0123456789abcdefghijklmnopqrstuvwxyz [expr $length / 36 + 1]] 1 $length]\n"
            }
            "wait" {
                set wait [checkNum [lindex $l 1]]
                after $wait [list sendReply $sock "Done\n"]
            }
            "start" {
                set wait [checkNum [lindex $l 1]]
                set ::counter 0
                after $wait sendAsync $wait [list [lindex $l 2]]
                sendReply $sock "Started\n"
            }
            "stop" {
                set ::counter -1
                sendReply $sock "Stopped\n"
            }
            "set" {
                set ::values([lindex $a 1]) [lrange $l 2 end-1]
                sendReply $sock "Ok\n"
            }
            "get" {
                if [info exists ::values([lindex $l 1])] {
                    sendReply $sock "[lindex $l 1] $::values([lindex $l 1])\n"
                } else {
                    sendReply $sock "ERROR: [lindex $l 1] not found\n"
                }
            }
            "help" {
                sendReply $sock "help             this text\n"
                sendReply $sock "echo string      reply string\n"
                sendReply $sock "wait msec        reply Done after some time\n"
                sendReply $sock "start msec       start sending messages priodically\n"
                sendReply $sock "stop             stop sending messages\n"
                sendReply $sock "set key value    set a value\n"
                sendReply $sock "get key          reply value\n"
                sendReply $sock "disconnect       close connection\n"
            }
        }
    } msg] {
        sendReply $sock "ERROR: $msg\n"
        puts stderr $::errorInfo
    }
}

proc sendAsync {wait message} {
    if {$::counter < 0} return
    foreach term [array names ::socket] {
        sendReply $::socket($term) "Message number [incr ::counter] $message\n";
    }
    after $wait sendAsync $wait [list $message]
}

if {[info proc tkTextInsert] != ""} {
    set insert tkTextInsert
    set paste tkTextPaste
} else {
    set insert tk::TextInsert
    set paste tk_textPaste
}

rename $insert tkTextInsert_org
rename $paste tkTextPaste_org

proc $insert {w s} {
    global socket
    if {[string equal $s ""] || [string equal [$w cget -state] "disabled"]} {
        return
    }
    sendReply $socket($w) $s
}

proc $paste {w x y} {
    global insert
    set s [selection get -displayof $w]
    $insert $w $s
}

#remove bindings on Control-<letter>
for {set ascii 0x61} {$ascii <= 0x7a} {incr ascii} {
    bind Text <Control-[format %c $ascii]> ""
}
#remove bindings on symbolic tags
foreach tag {Clear Paste Copy Cut} {
    bind Text <<$tag>> ""
}

bind Text <Control-Key> [list $insert %W %A]
