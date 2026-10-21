# Picotility

A **memory-constrained PICO-8 emulator** for [Tactility OS](https://github.com/TactilityProject/Tactility).

## Goals

- **≤100 KB RAM** - Run on base ESP32 without PSRAM
- **High compatibility** - Support 90%+ of PICO-8 games  
- **Multi-cart support** - Switch between preloaded carts
- **SD card loading** - Load .p8 carts from filesystem
- **Touch controls** - Virtual gamepad overlay

## Building

```bash
# Optional: use a locally checked-out Tactility SDK
export TACTILITY_SDK_PATH=/path/to/TactilitySDK

# Download/use the SDK version from manifest.properties and build
# `tt.py` maps the SDK's native POSIX build to the simulator's arm64 package
# directory automatically on Apple Silicon.
./tt app build --architecture posix-arm64

# Build, install, and launch the POSIX app through a running simulator
./tt app bir --architecture posix-arm64 --host 127.0.0.1

# Build the ESP32 app
./tt app build --architecture esp32s3
```

### Tactility simulator

The POSIX SDK target builds Picotility as a loadable app, but it does not include the Tactility simulator itself. Build the simulator from the Tactility source:

```bash
git clone --recurse-submodules https://github.com/TactilityProject/Tactility.git
cd Tactility
env -u ESP_IDF_VERSION -u IDF_PATH -u IDF_TARGET -u CMAKE_TOOLCHAIN_FILE \
  cmake -S . -B buildsim -G Ninja
PATH="/tmp/tactility-python-bin:$PATH" \
  env -u ESP_IDF_VERSION -u IDF_PATH -u IDF_TARGET -u CMAKE_TOOLCHAIN_FILE \
  cmake --build buildsim --target Tactility
./buildsim/Tactility/Tactility
```

After the simulator boots, open its built-in **Development** app and enable the development server. It will then listen on `127.0.0.1:6667`. Leave the
simulator running, then run:

```bash
./tt app bir --architecture posix-arm64 --host 127.0.0.1
```

Use `./tt host <address>` to save a different host.

### Bundled PNG carts

Keep the dynamically loaded app binary separate from bundled content. Put
`.p8.png` or `.p8` carts in this project directory:

```text
assets/carts/MyGame.p8.png
```

They are copied into the installed app package and appear in Picotility's cart
list. The simulator's installed directory under `Data/tactility/app` is
generated output and may be replaced on every install; do not use it as the
source location for assets. For carts added after installation, copy them to
the user cart directory shown in the app:

```text
/sdcard/tactility/picotility/
```

## Credits

Inspired by and learning from:

- [fake-08](https://github.com/jtothebell/fake-08) - Portable PICO-8 implementation
- [PicoPico](https://github.com/DavidVentura/PicoPico) - ESP32 PICO-8 player
- [z8lua](https://github.com/samhocevar/z8lua) - PICO-8 compatible Lua

## License

MIT License - See [LICENSE](LICENSE)
