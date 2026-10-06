# Contributing to CFG_bench

## Reporting bugs and asking questions

Open an issue on [GitHub Issues](https://github.com/homka122/CFG_bench/issues).
For a bug, include:

- the command you ran, for example `./build/cfg_bench -t -c configs/for_test.csv -a CFL_adv -efbl`
- the config row and where the graph and grammar come from
- the full output
- the GraphBLAS version and the LAGraph commit you built

For questions you can also write to [@homka122](https://t.me/homka122) on Telegram.

## Making changes

1. Set up the dependencies as described in [Requirements](README.md#requirements)
   and [Usage](README.md#usage), then run `make hooks` once to install the pre-commit hooks.
2. Create a branch from `main`.
3. Write commit messages as one line in the form `Type: short description`,
   for example `Fix: skip blank config lines`. Types used in the history:
   `Feat`, `Fix`, `Refactor`, `Docs`, `CI`, `Build`, `Style`.
4. Before opening a pull request, run:
   - `make lint` — formatting with clang-format and ruff (`make format` fixes it)
   - `make CI` — every algorithm on `configs/for_test.csv`
5. Open a pull request to `main`. CI runs the same checks.

The algorithms themselves live in LAGraph. Changes to them go to the
[LAGraph](https://github.com/SparseLinearAlgebra/LAGraph) repository, and CFG_bench
is built against the `homka122/all_algorithms_benchmark` branch.

## Adding an algorithm

Each algorithm is wrapped in an adapter in `src/adapters/`:

1. Create `adapter_<name>.h` and `adapter_<name>.c` that implement `AdapterMethods`
   and provide `adapter_<name>_get_methods()`.
2. Add a row to the `algorithms` table in `src/adapters/registry.c`: the name for `-a`,
   the getter, a prepare function and whether the algorithm is multiple-source.
   The prepare function builds the adapter's `PrepareData` from `AlgorithmOptions`;
   use `prepare_no_data` if the adapter has none. The `-a` option and the help text
   take the name from the table, so `src/test.c` doesn't change.
3. Add a run of the algorithm to the `CI` target in the `Makefile`.

## Technical documentation

- [README](README.md): CLI options, config, grammar and graph formats
- [`src/adapters/adapter.h`](src/adapters/adapter.h): the adapter interface and its lifecycle
- [`data/grammars/README.md`](data/grammars/README.md): RSM templates of the grammars
