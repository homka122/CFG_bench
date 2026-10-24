Use the `configs/for_test.csv` configuration file when running tests.

Graph files (`.g`, `.txt`) and some grammar files (`.cnf`) can be very large. Always check a file's size before reading it in full.

As the final check, run the affected algorithms on the reduced dataset `tests/data/configs/all.csv` (one config per dataset is also in `tests/data/configs/`).
