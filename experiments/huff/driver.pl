#!/usr/bin/perl

@out = `time ./huff-e-malloc tests/bigtest.in`;
print "***@out***\n";

# 0.77user 0.02system 0:00.80elapsed 100%CPU (0avgtext+0avgdata 1840maxresident)k
if ($out =~ m/.*([0-9\.]+)user ([0-9\.]+)system ([0-9\.\:]+)elapsed.*/g) {
#  print "\n$2 $3 $4\n";
}
else {
#  print "not\n";
}
