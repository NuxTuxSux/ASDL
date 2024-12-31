# ASDL
## A (very) simple program to enable graphic programming when no other ways are easily viable.

![Screenrecording of examples/balls.k](bs.gif)

It works listening to two ports: one for custom graphic commands and the other for sending events.
I am primarly developing it to enable simple games/graphic programming in [K](https://codeberg.org/ngn/k) and APL.

Any contribution is welcomed :)

## Currently accepted commands:
- ```]```: Begin (and clear) frame to render
- ```SC r g b```: Set drawing color
- ```DL x_1 y_1 ... x_n y_n```: Draw line(s)
- ```FR x_1 y_1 w_1 h_1 ... x_n y_n w_n h_n```: Fill rect(s)
- ```FC x_1 y_1 r_1 ... x_n y_n r_n```: Fill circle(s)

## Ports:
- 1234: Waits for graphic commands
- 1235: Sends keystrokes

## Example program (in ngn/k)
```
res:1024 768
dt:1000
S:{t:x+`t[];(t>){`t[]}/0n;}

a:0 3.0                                      / acceleration
rrng:20 50                                   / radii range
n:200                                        / n. of balls
cls:(n;3)#*[3;n]?255                         / colors
rs:rrng[0]+n?-/|rrng                         / radii
cts:+n?/:res                                 / initial centers
vs:8*(n;2)#-0.5+?n*2                         / velocities

keyb:<`":12346"
cmdf:<`":12345"
C:{cmdf 0:x}

dB:{C"SC ",`k@x;C"FC ",`k@y}                 / draw ball
bv:{-1 1@/:&[res>x+y;x>y]}                   / bounces

upd:{cts::&[res-\:/:rs;]rs|cts+vs;vs::a+/:vs*bv'[cts+vs;rs+3]}
main:{tt:upd[];tt:C@,"[";tt:dB'[cls;_cts,'_rs];C@,"]"}

1000 main/0n
```