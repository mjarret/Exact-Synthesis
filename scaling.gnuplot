set datafile separator comma
set grid
set term pngcairo size 1200,800
set output 'scaling.png'
set xlabel 'T'
set ylabel 'Runtime (s)'
set y2label 'Delta Memory (MB)'
set ytics nomirror
set y2tics
set logscale y
set logscale y2
eps_rt = 1e-9
eps_mb = 1.0/(1024.0*1024.0)
plot 'scaling.csv' using 1:( $2>0 ? $2 : eps_rt ) axes x1y1 with linespoints title 'Runtime (s)',\
     'scaling.csv' using 1:( ($3>0 ? ($3/1024.0/1024.0) : eps_mb) ) axes x1y2 with linespoints title 'Delta Mem (MB)'
