\l asdl.k

res:1024 768
a:0 3.0                                      / acceleration
rrng:20 50                                   / radii range
n:200                                        / n. of balls

cls:(n;3)#*[3;n]?255                         / colors
rs:rrng[0]+n?-/|rrng                         / radii
cts:+n?/:res                                 / initial centers
vs:8*(n;2)#-0.5+?n*2                         / velocities

dB:{SC@x;FC@y}                               / draw ball
bv:{-1 1@/:&[res>x+y;x>y]}                   / bounces

upd:{cts::&[res-\:/:rs;]rs|cts+vs;vs::a+/:vs*bv'[cts+vs;rs]}
main:{upd[];BF[];dB'[cls;_cts,'_rs];EF[]}

/ 100 main/0n
