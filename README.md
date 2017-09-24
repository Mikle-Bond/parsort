Parallel sort
-------------

This is parallel merge sort using MPI.

## Compiling

Building main executable can be done like this 

	$ mpicc -o parsort -Wall -Wextra -O2 parallel_sort.c

Also there is a supporting utility in conbin, used in random input generator.
It doesn't requre anything special, just

	$ gcc -o conbin.out conbin.c

## Usage

Due to complications in reading files in MPI, the input files have to be binary.
The input file should meet this requirements:
 - containes binary representation of 4-byte integers;
 - first integer is the size of sorted array;
 - next are integers, not less, then specified in first parameter.

To make it easyer to operate this format there are several tools available.
To generate a random input use gen_input.sh like this

	$ ./gen_input.sh 10000 new_input.bin

After this it can be seen with `od` command like this

	$ od -An -i new_input.bin | less

or any other hexdump-like command. 

Another way is to create binary input file is form the list of integers in text format. In this case
use something like this

	$ ( wc -w input.txt ; cat input.txt ) | ./conbin.out > input.bin

Finaly, invoking parallel sort after this can be something like

	$ mpirun -np 8 ./parsort new_input.bin new_output.bin

The output files are also binary, and can be viewed in the same way as input files.
Note that amount of elements is not in the output file.


