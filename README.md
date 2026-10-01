# Project 1: Concurrent Prefix Sum

## Team members

- Thai Tong
- Hoa Doan

## Upload to CS1 and run the sample

Use `scp` to copy the project folder from your computer to CS1. Replace
`your_netid` with your UTD NetID in the commands below.
Replace `/path/to/Project 1` with the path to your local project folder.
The commands below assume the folder is named `Project 1`; adjust the remote
`cd` command if your folder has a different name.

1. In your **local terminal**, upload the folder:

   ```sh
   scp -r "/path/to/Project 1" your_netid@cs1.utdallas.edu:~/
   ```

2. Connect to CS1 and complete the login prompts:

   ```sh
   ssh your_netid@cs1.utdallas.edu
   ```

3. Once logged in, open the copied folder, rebuild for CS1, and run the sample:

   ```sh
   cd "$HOME/Project 1"
   make clean
   make
   ```

`make` compiles the original version, runs the sample, and displays the output:

```text
1 3 6 10 15 21 28 36
```

To run the bonus version after uploading, follow the **Bonus version** section
below.

## Other department machines

To use CS2 or giant, replace `cs1.utdallas.edu` in the upload command with the
chosen hostname, then connect using the corresponding command:

```sh
ssh your_netid@cs2.utdallas.edu
```

```sh
ssh your_netid@giant.utdallas.edu
```

After logging in, open the copied project directory:

```sh
cd "$HOME/Project 1"
```

## Compile the program

In the project directory on the department machine, run the following commands
to compile a fresh executable:

```sh
make build
```

This creates the executable `my-sum` using the department machine's C++ compiler.

## Run the compiled program

On the same department machine, from the directory containing `my-sum`, run:

```sh
./my-sum n m input_file output_file
```

- `n`: number of input integers to process (a positive integer).
- `m`: number of worker processes, with `1 <= m <= n`.
- `input_file`: path to a file containing at least `n` whitespace-separated
  signed 64-bit integers. Extra integers are ignored.
- `output_file`: path where the program writes the `n` prefix sums. An existing
  file at this path is overwritten.

For example, the included `input.txt` contains `1 2 3 4 5 6 7 8`.
Run it with eight elements and five workers, then display the result:

```sh
./my-sum 8 5 input.txt output.txt
cat output.txt
```

Expected contents of `output.txt`:

```text
1 3 6 10 15 21 28 36
```

The program writes results to the output file. Errors are printed to standard
error and cause a nonzero exit status.

## Other Makefile commands

- `make`: compile the original version, run the included example, and display `output.txt`.
- `make build`: compile `my-sum.cpp` as the executable `my-sum`.
- `make bonus`: compile `my-sum-bonus.cpp` as the executable `my-sum`.
- `make clean`: remove the compiled executable.

## Bonus version

`my-sum-bonus.cpp` implements a reusable barrier with two shared counters
(O(1) barrier storage) and alternates between two array buffers (O(n) total
algorithm storage). Workers announce arrival in worker-ID order using only
shared-memory reads and writes. The last worker resets the arrival counter
and advances the generation to release the other workers. Each round still
uses the Hillis–Steele update and evenly divided work.

On a department machine, compile and run the bonus version with:

```sh
make bonus
./my-sum 8 5 input.txt output.txt
cat output.txt
```

`make bonus` builds the separate bonus source as `my-sum`, the executable name
required by the assignment. To switch back to the original version, run
`make build`. The original source is `my-sum.cpp`.
# CS-4348---Project-1
