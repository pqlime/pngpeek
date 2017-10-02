/* This program is free software. It comes without any warranty, to
     * the extent permitted by applicable law. You can redistribute it
     * and/or modify it under the terms of the Do What The Fuck You Want
     * To Public License, Version 2, as published by Sam Hocevar. See
     * http://www.wtfpl.net/ for more details. */

#ifndef INCLUDE_PNGPEEK
#define INCLUDE_PNGPEEK

#include <stdint.h>

void pngpeek_seed(uint32_t seed);
uint8_t pngpeek_randomu8();
void pngpeek_encodeImage(uint8_t* imData, size_t imSize, uint8_t* inMsg,
                         size_t inSize);
void pngpeek_decodeImage(uint8_t* imData, size_t imSize, uint8_t** outMsg,
                         size_t* outSize);

#endif // INCLUDE_PNGPEEK
