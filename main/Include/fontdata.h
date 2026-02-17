#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <string>
std::string get_font_data();
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t defaultFontBinaryData[2048];

#ifdef __cplusplus
}
#endif
