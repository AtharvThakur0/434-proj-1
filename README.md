To run from scratch, execute the following:
make clean && make && ./main {L} {H} {NP}

The log and trace files provided in the repository were generated with L=120000, H=150 and NP=13

NP = (1 + m + m^2 + ...) where m is the B-tree parameter (child count per node) and the amount of terms in the summation is the tree height. This ensures full tree levels.

Note that log.txt does not contain the pstree trees due to technical limitations. Please view these by running the program.
