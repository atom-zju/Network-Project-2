#!/bin/bash


files=$(ls *.test)
files+=' simpletest1'
files+=' simpletest2'
files+=' simpletest3'

echo "Test files are:"
for f in ${files}
do 
	echo $f
done
echo

for f in ${files}
do
	echo Simulating test file: $f in DV mode 
	../Simulator $f DV > $f"_DV.out"
	wait
done

for f in ${files}
do
	echo Simulating test file: $f in LS mode 
        ../Simulator $f LS > $f"_LS.out"
        wait
done
