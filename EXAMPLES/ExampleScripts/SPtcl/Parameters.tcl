#================================================================
# Parameters.tcl
# --Set up some general parameters
# Zhaohui Yang and Boris Jeremic (@ucdavis.edu)
# March 31, 2002
#================================================================

# get proper units for everything
source Units.tcl

set PI [expr 4*atan(1.0)];
set g [expr -9.81*$m/pow($sec, 2)];	# gravitational constant in m/sec^2
