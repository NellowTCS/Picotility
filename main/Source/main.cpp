// main.cpp — Picotility: PICO-8 Emulator for Tactility
#include "Picotility.h"
#include <TactilityCpp/App.h>

extern "C" {

int main(int argc, char* argv[]) {
    registerApp<Picotility>();
    return 0;
}

}
