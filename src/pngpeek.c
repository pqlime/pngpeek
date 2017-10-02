/* This program is free software. It comes without any warranty, to
     * the extent permitted by applicable law. You can redistribute it
     * and/or modify it under the terms of the Do What The Fuck You Want
     * To Public License, Version 2, as published by Sam Hocevar. See
     * http://www.wtfpl.net/ for more details. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../lib/lodepng.h"
#include "pngpeek.h"

#define PNGPEEK_HEADER_SIZE 32 // Header contains the CRC code (32-bit, so 4 px)
#define CRC_SEED_SHIFT 0 // For extra seeding

static uint32_t __pngpeek_state = 0x951EE51C;

void pngpeek_seed(uint32_t seed)
{
    __pngpeek_state = seed;
}

uint8_t pngpeek_randomu8()
{
    // Taken from Wikipedia's page on the xorshift algorithm
    uint32_t x = __pngpeek_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    __pngpeek_state = x;
    return (uint8_t)(x);
}

// Determine which bit/RGB order to use through the random int
static uint8_t pngpeek_getP(uint8_t cS, uint8_t c)
{
    uint8_t bitIndex = 0;
    switch(cS)
    {
        case 0:
            switch(c)
            {
                case 0:
                    bitIndex = 6;
                    break;
                case 1:
                    bitIndex = 3;
                    break;
                case 2:
                    bitIndex = 0;
                    break;
            }
            break;
        case 1:
            switch(c)
            {
                case 0:
                    bitIndex = 3;
                    break;
                case 1:
                    bitIndex = 6;
                    break;
                case 2:
                    bitIndex = 0;
                    break;
            }
            break;
        case 2:
            switch(c)
            {
                case 0:
                    bitIndex = 0;
                    break;
                case 1:
                    bitIndex = 5;
                    break;
                case 2:
                    bitIndex = 3;
                    break;
            }
            break;
    }
    
    return bitIndex;
}

uint8_t pngpeek_readPixel(uint8_t* pixelPtr, uint8_t rByte)
{
    // Calculate RGB order and size
    uint8_t components[3] = {0, 0, 0};
    uint8_t specialComponent = rByte % 3; // Byte to use two bits instead of three
    components[0] = (rByte >> 3) % 3;
    components[1] = (components[0] + (rByte >> 2 & 0b1) + 1) % 3;
    components[2] = 3 - components[0] - components[1];
    
    // Read the byte
    uint8_t outByte = 0;
    
    for (uint8_t i = 0; i < 3; i++)
    {
        uint8_t component = components[2-i];
        uint8_t pixelValue = pixelPtr[component] & 0b111;
        uint8_t bitIndex = pngpeek_getP(specialComponent, component);

        outByte += pixelValue << bitIndex;
        bitIndex += (component == specialComponent ? 2 : 3);
    }
    
    return outByte;
}

static void pngpeek_writePixel(uint8_t* pixelPtr, uint8_t mByte, uint8_t rByte)
{
    // Calculate RGB order and size
    uint8_t components[3] = {0, 0, 0};
    uint8_t specialComponent = rByte % 0b11; // Byte to use two bits instead of three
    components[0] = (rByte >> 3) % 0b11;
    components[1] = (components[0] + (rByte >> 2 & 0b1) + 1) % 0b11;
    components[2] = 3 - components[0] - components[1];
    
    // Write the byte to RGB
    for (uint8_t i = 0; i < 3; i++)
    {
        uint8_t component = components[2-i];
        uint8_t bitIndex = pngpeek_getP(specialComponent, component);
        uint8_t val = (mByte >> bitIndex) & (specialComponent == component ? 0b11 : 0b111);

        pixelPtr[component] &= 0b11111000;
        pixelPtr[component] += val;
    }
}

void pngpeek_encodeImage(uint8_t* imData, size_t imSize, uint8_t* inMsg,
                         size_t inSize)
{
    // Ensure enough data exists in the image for encoding
    if (imSize < PNGPEEK_HEADER_SIZE + inSize * 4)
    {
        printf("Not enough space in image to hide data!\n");
        exit(EXIT_FAILURE);
    }
    
    uint32_t p_index = PNGPEEK_HEADER_SIZE; // PNG data index
    uint32_t d_index = 0; // In-data index
    
    // Calculate CRC of first 4 pixels w/o last 3 significant bits
    imData[0] &= 0b11111000;
    imData[1] &= 0b11111000;
    imData[2] &= 0b11111000;
    imData[3] &= 0b11111000;
    uint32_t crcCode = lodepng_crc32(imData, 4);
    pngpeek_seed(crcCode+CRC_SEED_SHIFT);
    
    // Write CRC & message size to data
    for (int i = 0; i < 4; i++)
    {
        pngpeek_writePixel(imData + i * 4, (crcCode >> (8*i)) & 0xFF, 0);
        pngpeek_writePixel(imData + 16 + i * 4, (inSize >> (8*i)) & 0xFF, 0);
    }
    
    printf("CRC %08x\n", crcCode);
    
    while ((p_index < imSize) && (d_index < inSize))
    {
        uint8_t* pixel = imData + p_index;
        uint8_t mByte = inMsg[d_index];
        uint8_t rByte = pngpeek_randomu8(); // Generate random character
        
        pngpeek_writePixel(pixel, mByte, rByte);
        
        p_index += 4; // Move to next pixel
        d_index++;
    }
}

void pngpeek_decodeImage(uint8_t* imData, size_t imSize, uint8_t **outMsg,
                         size_t* outSize)
{
    if (imSize < PNGPEEK_HEADER_SIZE)
    {
        printf("Image is impossibly small!\n");
        exit(EXIT_FAILURE);
    }
    
    uint32_t crcCode = 0;
    size_t   inSize = 0;
    
    // Read CRC code & size
    for (int i = 0; i < 4; i++)
    {
        crcCode += pngpeek_readPixel(imData + i * 4, 0) << (i * 8);
        inSize += pngpeek_readPixel(imData + 16 + i * 4, 0) << (i * 8);
    }
    
    printf("CRC %08x\n", crcCode);
    pngpeek_seed(crcCode+CRC_SEED_SHIFT); // Seed generator
    
    //Check that the CRC code is the same as in the file
    imData[0] &= 0b11111000;
    imData[1] &= 0b11111000;
    imData[2] &= 0b11111000;
    imData[3] &= 0b11111000;
    
    if (lodepng_crc32(imData, sizeof(uint32_t)) != crcCode)
    {
    //Check that the CRC code i
        printf("CRC lookup failed; not pngpeek encoded!\n");
        exit(EXIT_FAILURE);
    }
    
    // Allocate memory for the out message
    *outMsg = calloc(sizeof(uint8_t), inSize);
    if (!*outMsg)
    {
        printf("Couldn't malloc enough bytes for output buffer!\n");
    }
    
    // Read data
    uint32_t p_index = PNGPEEK_HEADER_SIZE;
    uint32_t d_index = 0;
    
    while ((p_index < imSize) && (d_index < inSize))
    {
        uint8_t* pixel = imData + p_index;
        uint8_t rByte = pngpeek_randomu8(); // Generate random character
        uint8_t outByte = pngpeek_readPixel(pixel, rByte);
        
        (*outMsg)[d_index] = outByte;
        p_index += 4;
        d_index++;
    }
    
    *outSize = inSize;
}
