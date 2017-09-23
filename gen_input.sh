#!/bin/bash
# Author: Mikle_Bond
# Usage: ./get_input.sh <amount of numbers> <output file>
# Description: Input generator of random numbers for parallel sort.
#	It gets $1 random numbers and converts them to binary format.
#	
#	See also: https://stackoverflow.com/questions/12939279
#

T2B="./conbin.out"

( 
	echo "$1"
	dd bs=4 count=$1 if=/dev/urandom of=/dev/stdout | od -An -t u4 
) | $T2B > "$2"


