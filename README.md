# NEWS

 A new release Themisto 3.0 is almost ready. The readme is outdated, working on it. For Themisto v2.1, go to commit [27bcca8](https://github.com/algbio/themisto/tree/27bcca83d25991f52211253a7c9a0a08436b5f65).

# About Themisto
Themisto is a succinct colored de Bruijn graph supporting pseudo-alignment against a database of reference sequences similar to the tool Kallisto. For more information, see the [webpage](https://www.helsinki.fi/en/researchgroups/genome-scale-algorithmics/themisto) and the [paper](https://www.biorxiv.org/content/biorxiv/early/2020/04/04/2020.04.03.021501/DC1/embed/media-1.pdf?download=true). The pseudoalignment algorithm is modeled after the tool Kallisto.

# Installation
## Requirements

We currently support only Linux and macOS. For compilation, you will need a C++20 compliant compiler with OpenMP support, CMake v3.1 or newer, and [Rust](https://www.rust-lang.org/tools/install). If compiling with g++, make sure that the version is at least g++-10, or you might run into compilation errors with the standard library &lt;filesystem&gt; header.

## Compiling

### Linux

These instructions have been tested to work on Ubuntu 18.04 with the aforementioned dependencies installed. To build the software, enter the Themisto directory and run

```
git submodule init
git submodule update
cd build
cmake .. -DMAX_KMER_LENGTH=31 -DCMAKE_BUILD_ZLIB=1 -DCMAKE_BUILD_BZIP2=1
make
```

If there is a linking error at the very end, try runnning `make` again. Where 31 is the maximum k-mer length (node length) to support, up to 255. The larger the k-mer length, the more time and memory the index construction takes. Values that are one less than a multiple of 32 work the best. This will create the binary at`build/bin/themisto`.

**Troubleshooting**: If you run into problems involving the &lt;filesystem&gt; header, you probably need to update your compiler. The compiler `g++-10` should be sufficient. Install a new compiler and direct CMake to use it with the `-DCMAKE_CXX_COMPILER` option. For example, to set the compiler to `g++-10`, run CMake with the option `-DCMAKE_CXX_COMPILER=g++-10`. 

## MacOS

Warning: these instruction have not been tested in a while. We are aware that there are problems with compiling on ARM-based Macbooks due to x86 instructions in the dependency KMC. Currently, we have no fix for this.

Compiling Themisto on macOS requires users to first install gcc-10 or newer from homebrew with

```
brew install gcc@10
```

Afterwards, Themisto can be compiled by entering the directory and running

```
git submodule init
git submodule update
cd ggcat/crates/capi/ggcat-cpp-api/
make
cd ../../../..
cd build
cmake -DCMAKE_C_COMPILER=$(which gcc-10) -DCMAKE_CXX_COMPILER=$(which g++-10) -DMAX_KMER_LENGTH=31 ..
make
```

Where 31 is the maximum k-mer length to support, up to 255. The larger the k-mer length, the more time and memory the index construction takes. If you run into problems involving zlib or bzip2, add `-DCMAKE_BUILD_ZLIB=1 -DCMAKE_BUILD_BZIP2=1` into the cmake command.

Note that macOS has a very small limit for the number of concurrently opened files. Themisto can use temporary files to conserve RAM, and may run into this limit. To increase the limit, run the command

```
ulimit -n 2048
```

# Usage

## Quick start

To build the Themisto index for a set of genomes, you need to pass in a text file that contains the paths to the FASTA files of the genomes, one file per line. Each FASTA file is given a different color 0,1,2,3... in the same order as they appear in the list. There are three example genomes of E. coli in `example_input` and a file at `example_input/coli_file_list.txt` listing the file names. To build the index for this data, run the following command:

```
./build/bin/themisto build -k 31 -i example_input/coli_file_list.txt --index-prefix my_index --temp-dir temp --mem-megas 2048 --n-threads 4 --file-colors --reverse-complements
```

This builds an index with k = 31, such that the index files are written to `my_index.tdbg` and `my_index.tcolors`, using the directory `temp` as temporary storage, using four threads and up to 2GB of memory. The flag `--reverse-complements` adds the reverse complements of all k-mers to the index. We recommend to use a fast SSD drive for the temporary directory.

To align the four sequences in `example_input/queries.fna` against the index we just built, writing output to `out.txt` run:

```
./build/bin/themisto pseudoalign --query-file example_input/queries.fna --index-prefix my_index --temp-dir temp --out-file out.txt --n-threads 4 --threshold 0.7 --ignore-unknown-kmers
```

This reports all colors such that at least a fraction 0.7 of the k-mers of the query are in the reference genome of the color, ignoring k-mers that are not found in any reference.

This should produce the following output file:

```
0 0 2
1 0 1 2
2 2
3 2
```

There is one line for each query sequence. The lines may appear in a different order if parallelism was used. The first integer on a line is the 0-based rank of a query sequence in the query file, and the rest of the integers are the colors that are pseudoaligned with the query. For example, here the query with rank 1 (i.e. the second sequence in the query file) pseudoaligns to colors 0, 1 and 2.

## Full instructions for index construction

```
Usage:
  build [OPTION...]

  -k, --node-length arg         The k of the k-mers. (default: 0)
  -i, --input-file arg          The input sequences in FASTA or FASTQ 
                                format. The format is inferred from the 
                                file extension. Recognized file extensions 
                                for fasta are: .fasta, .fna, .ffn, .faa and 
                                .frn . Recognized extensions for fastq are: 
                                .fastq and .fq. (default: "")
  -c, --manual-colors arg       A file containing one integer color per 
                                sequence, one color per line. If there are 
                                multiple sequence files, then this file 
                                should be a text file containing the 
                                corresponding color filename for each 
                                sequence file, one filename per line.  
                                (default: "")
  -f, --file-colors             Creates a distinct color 0,1,2,... for each 
                                file in the input file list, in the order 
                                the files appear in the list
  -e, --sequence-colors         Creates a distinct color 0,1,2,... for each 
                                sequence in the input, in the order the 
                                sequences are processed. This is the 
                                default behavior if no other color options 
                                are given.
      --no-colors               Build only the de Bruijn graph without 
                                colors.
  -o, --index-prefix arg        The de Bruijn graph will be written to 
                                [prefix].tdbg and the color structure to 
                                [prefix].tcolors.
  -r, --reverse-complements     Also add reverse complements of the k-mers 
                                to the index.
      --temp-dir arg            Directory for temporary files. This 
                                directory should have fast I/O operations 
                                and should have as much space as possible.
  -m, --mem-megas arg           Number of megabytes allowed for external 
                                memory algorithms (must be at least 2048). 
                                (default: 2048)
  -t, --n-threads arg           Number of parallel exectuion threads. 
                                Default: 1 (default: 1)
      --randomize-non-ACGT      Replace non-ACGT letters with random 
                                nucleotides. If this option is not given, 
                                k-mers containing a non-ACGT character are 
                                deleted instead.
  -d, --colorset-pointer-tradeoff arg
                                This option controls a time-space tradeoff 
                                for storing and querying color sets. If 
                                given a value d, we store color set 
                                pointers only for every d nodes on every 
                                unitig. The higher the value of d, the 
                                smaller then index, but the slower the 
                                queries. The savings might be significant 
                                if the number of distinct color sets is 
                                small and the graph is large and has long 
                                unitigs. (default: 1)
      --load-dbg                If given, loads a precomputed de Bruijn 
                                graph from the index prefix. If this is 
                                given, the value of parameter -k is ignored 
                                because the order k is defined by the 
                                precomputed de Bruijn graph.
  -s, --coloring-structure-type arg
                                Type of coloring structure to build 
                                ("sdsl-hybrid", "roaring"). (default: 
                                sdsl-hybrid)
      --from-index arg          Take as input a pre-built Themisto index. 
                                Builds a new index in the format specified 
                                by --coloring-structure-type. This is 
                                currenlty implemented by decompressing the 
                                distinct color sets in memory before 
                                re-encoding them, so this might take a lot 
                                of RAM. (default: "")
  -v, --verbose                 More verbose progress reporting into 
                                stderr.
      --silent                  Print as little as possible to stderr (only 
                                errors).
  -h, --help                    Print usage
```

## Full instructions for `pseudoalign`

This command aligns query sequences against an index that has been built previously. The output is one line per input read. Each line consists of a space-separated list of integers. The first integer specifies the rank of the read in the input file, and the rest of the integers are the identifiers of the colors of the sequences that the read pseudoaligns with. If the program is ran with more than one thread, the output lines are not necessarily in the same order as the reads in the input file. This can be fixed with the option --sort-output.

The query can be given as one file, or as a file with a list of files. In the former case, one output file can be specified with the option --out-file. If --out-file is not given, the results will be print to cout. In the latter case, we must give a file that lists one output filename per line using the option --out-file-list.

The query file(s) should be in fasta of fastq format. The format is inferred from the file extension. Recognized file extensions for fasta are: .fasta, .fna, .ffn, .faa and .frn . Recognized extensions for fastq are: .fastq and .fq

```
Usage:
  pseudoalign [OPTION...]

  -q, --query-file arg         Input file of the query sequences (default: 
                               "")
      --query-file-list arg    A list of query filenames, one line per 
                               filename (default: "")
  -o, --out-file arg           Output filename. Print results if no output 
                               filename is given. (default: "")
      --out-file-list arg      A file containing a list of output 
                               filenames, one per line. (default: "")
  -i, --index-prefix arg       The index prefix that was given to the build 
                               command.
      --temp-dir arg           Directory for temporary files.
      --threshold arg          Run a thresholded pseudoalignment, i.e. 
                               report all colors that match to at least the 
                               given fraction k-mers in the query. If not 
                               given, runs intersection pseudoalignment. 
                               (default: -1.0)
      --ignore-unknown-kmers   Ignore in thresholded pseudoalignment all 
                               k-mers that are not found in the de Bruijn 
                               graph, or that have no colors. The 
                               intersection pseudoalignment always ignores 
                               unknown k-mers.
      --rc                     Also pseudoalign against the reverse 
                               complement of the query. Note: If the 
                               reverse complements were added to the index 
                               with the option --reverse complements in 
                               themisto build, then this option has no 
                               effect on the pseudoalignment and the 
                               program does unnecessary work. 
  -t, --n-threads arg          Number of parallel exectuion threads. 
                               Default: 1 (default: 1)
      --gzip-output            Compress the output files with gzip.
      --sort-output            Sort the lines of the out files by sequence 
                               rank in the input files.
      --buffer-size-megas arg  Size of the input buffer in megabytes in 
                               each thread. If this is larger than the 
                               number of nucleotides in the input divided 
                               by the number of threads, then some threads 
                               will be idle. So if your input files are 
                               really small and you have a lot of threads, 
                               consider using a small buffer. (default: 
                               8.0)
  -v, --verbose                More verbose progress reporting into stderr.
      --silent                 Print as little as possible to stderr (only 
                               errors).
  -h, --help                   Print usage

```

Examples:

Pseudoalign example_input/queries.fna against an index and print results:
```
./build/bin/themisto pseudoalign --query-file example_input/queries.fna --index-prefix my_index --temp-dir temp
```

Pseudoalign example_input/queries.fna against an index and write results to out.txt:
```
./build/bin/themisto pseudoalign --query-file example_input/queries.fna --index-prefix my_index --temp-dir temp --out-file out.txt
```

Pseudoalign a list of fasta files in input_list.txt into output filenames in output_list.txt:
```
./build/bin/themisto pseudoalign --query-file-list input_list.txt --index-prefix my_index --temp-dir temp --out-file-list output_list.txt
```

Pseudoalign example_input/queries.fna against an index using also reverse complements:
```
./build/bin/themisto pseudoalign --rc --query-file example_input/queries.fna --index-prefix my_index --temp-dir temp --outfile out.txt
```

## Extracting unitigs with `extract-unitigs`

This command dumps the unitigs and optionally their colors out of an existing Themisto index.

```
Usage:
  extract-unitigs [OPTION...]

  -i, --index-prefix arg  The index prefix that was given to the build 
                          command.
      --fasta-out arg     Output filename for the unitigs in FASTA format 
                          (optional). (default: "")
      --gfa-out arg       Output the unitig graph in GFA1 format 
                          (optional). (default: "")
      --colors-out arg    Output filename for the unitig colors (optional). 
                          If this option is not given, the colors are not 
                          computed. Note that giving this option affects 
                          the unitigs written to unitigs-out: if a unitig 
                          has nodes with different color sets, the unitig 
                          is split into maximal segments of nodes that have 
                          equal color sets. The file format of the color 
                          file is as follows: there is one line for each 
                          unitig. The lines contain space-separated 
                          strings. The first string on a line is the FASTA 
                          header of a unitig (without the '>'), and the 
                          following strings on the line are the integer 
                          color labels of the colors of that unitig. The 
                          unitigs appear in the same order as in the FASTA 
                          file. (default: "")
      --min-colors arg    Extract maximal unitigs with at least (>=)
                          min-colors in each node. Can't be used with
                          --colors-out. (optional) (default: 0)
  -v, --verbose           More verbose progress reporting into stderr.
  -h, --help              Print usage
```

## Extracting index statistics with `stats`

This command prints various statistics about the k-mers and colors in an existing index.

```
Usage:
  stats [OPTION...]

  -i, --index-prefix arg  The index prefix that was given to the build 
                          command.
      --unitigs           Also compute statistics on unitigs. This takes a 
                          while and requires the temporary directory to be 
                          set.
      --temp-dir arg      Directory for temporary files.
  -h, --help              Print usage
```

## Dumping the color matrix with `dump-color-matrix`

This command prints a file where each line corresponds to a k-mer in the index. The line starts with the k-mer, followed by space, followed by the color set of that k-mer. If `--sparse` is given, the color set is printed as a space-separated list of integers. Otherwise, the color set is printed as a string of zeroes and ones such that the i-th character is '1' iff color i is present in the color set.

Example:

```
./build/bin/themisto dump-color-matrix -i my_index -o dump.txt --sparse
```

Full instructions:

```
Usage:
  dump-color-matrix [OPTION...]

  -i, --index-prefix arg  The index prefix that was given to the build 
                          command.
  -o, --output-file arg   The output file for the dump.
  -v, --verbose           More verbose progress reporting into stderr.
      --silent            Print as little as possible to stderr (only 
                          errors).
      --sparse            Print only the indices of non-zero entries.
  -h, --help              Print usage
```


# For developers: building the tests

```
git submodule init
git submodule update
cd googletest
mkdir build
cd build
cmake ..
make
cd ../../build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_THEMISTO_TESTS=1
make
```

This builds the tests to `build/bin/themisto_tests`. The test executable must be ran at the root of the repository, or otherwise it wont find the test input files at `example_input`.

# License

This software is licensed under GPLv2. See LICENSE.txt.
