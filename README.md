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
# Set up Tactility SDK
export TACTILITY_SDK_PATH=/path/to/TactilitySDK

# Build
idf.py build

# Deploy to device
python tactility.py deploy
```

## Credits

Inspired by and learning from:

- [fake-08](https://github.com/jtothebell/fake-08) - Portable PICO-8 implementation
- [PicoPico](https://github.com/DavidVentura/PicoPico) - ESP32 PICO-8 player
- [z8lua](https://github.com/samhocevar/z8lua) - PICO-8 compatible Lua

## License

MIT License - See [LICENSE](LICENSE)
