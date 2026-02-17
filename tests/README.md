# Picotility Cart Tests

Test suite for the Picotility PICO-8 emulator using carts from `carts/` directory.

## Building

```bash
cd tests
mkdir -p build && cd build
cmake ..
make
```

## Running

From the project root directory:

```bash
./tests/build/test_runner
```

Or use the script:

```bash
./tests/run_tests.sh
```

## Test Options

- First argument: max frames to run per cart (default: 100)

```bash
./tests/build/test_runner 50  # Run 50 frames max per cart
```