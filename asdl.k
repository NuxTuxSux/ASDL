S:{t:x+`t[];(t>){`t[]}/0n;}                                  / sleep

evts:<`":12346"                                              / event receiving socket
cmds:<`":12345"                                              / command sending socket
C:{cmds 0:x}                                                 / send command
E:{evts 0:"GET";{x:" "\x;(`s$*x;`I$1_x)}'0:evts}             / receive and parse events

BF:{C@,"["}                                                  / start drawing new frame
EF:{C@,"]"}                                                  / render frame
SC:{C"SC ",`k@x}                                             / set color
FR:{C"FR ",`k@x}                                             / fill rect(s)
FC:{C"FC ",`k@x}                                             / fill circle(s)
DL:{C"DL ",`k@x}                                             / draw line(s)


/ TODO
/ - define a ns?
/ - define callbacks
/ - define init calling ASDL exc and then passing res
