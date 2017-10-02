/* This program is free software. It comes without any warranty, to
     * the extent permitted by applicable law. You can redistribute it
     * and/or modify it under the terms of the Do What The Fuck You Want
     * To Public License, Version 2, as published by Sam Hocevar. See
     * http://www.wtfpl.net/ for more details. */

/* Notes:
 * Each byte stores one pixel; the byte being stored in the LSB of the byte.
 * Two components of the pixel use 3 bits and one component uses 2.
 * For each pixel, a random integer is generated to determine which components
 * are in which order and which component uses the 2 bit.
 */

#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../lib/lodepng.h"
#include "pngpeek.h"

#define INITIAL_SEED 0x951EE51C

#define DO_DECODE 0
#define DO_ENCODE 1

#define PROG_NAME "pngpeek"

#define msg_defaultMsg "\
Usage: " PROG_NAME " --png=PATH out\n\
Use -e to encode files, otherwise decodes a PNG file\n\
"

#define msg_helpMsg "\
"PROG_NAME " - Secret message encoder/decoder\n\
-h, --help            displays this message\n\
-e, --encode          sets pngPeek to encode mode\n\
-i, --input           declares what input file to use (encode mode)\n\
-p, --png             declares the PNG file to use\n\
"

static const uint8_t msg_invalidFlagMsg[] = "\
Invalid flag \"%s\"; exiting\n";

static struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"encode", no_argument, NULL, 'e'},
    {"input", required_argument, NULL, 'i'},
    {"png", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}
};

// parseArgs - Goes through the program's arguments via getopt and gets values
void parseArgs(int argc, char **argv, uint8_t* encodeFlag, uint8_t** inPath,
               uint8_t** pngPath, uint8_t** outPath)
{   
    uint32_t opt     = 0;
	int32_t index    = 0;
    uint8_t success  = 0;
    
    if (argc < 2) // No args; print default and exit
    {
        printf(msg_defaultMsg);
        exit(EXIT_SUCCESS);
    }
    
    // Parse arguments
    while 
    ((opt = getopt_long(argc, argv, "h:ei:p:", long_options, &index)) != -1)
    {
        switch(opt)
        {
            case 'h': // Help
                printf(msg_helpMsg);
                exit(EXIT_SUCCESS);
            case 'e': // Set to encoder mode
                *encodeFlag = DO_ENCODE;
                break;
            case 'i': // Input file
                *inPath = optarg;
                break;
            case 'p': // Output file
                *pngPath = optarg;
                break;
        }
    }
    
    if (optind < argc) // Grab the in-file at the end of the message
    {
        *outPath = argv[optind];
        optind++;
    }
    
    if (!*pngPath)
    {
        printf("No PNG path given; exiting.\n");
        exit(EXIT_FAILURE);
    }
    
    if (!*outPath)
    {
        printf("No output file given; exiting.\n");
        exit(EXIT_FAILURE);
    }
    
    if (*encodeFlag && !*inPath) // Kill if encoding can't occur right
    {
        printf("Using encode mode w/o input file; exiting.\n");
        exit(EXIT_FAILURE);
    }
}

// loadPng - Loads a PNG's pixels given a path
void loadPng(uint8_t* path, uint8_t** data, size_t* data_s,
             uint32_t* width, uint32_t* height)
{   
    uint32_t pngError = 0;  // Error returned when loading png/pixels
    uint8_t* pngFile_d;     // PNG file data
    size_t  pngFile_s;      // PNG file size
    
    // Load the PNG file data
    pngError = lodepng_load_file(&pngFile_d, &pngFile_s, path);

    if (pngError)
    {
        printf("Failed to load image (path \"%s\")\n", path);
        printf("Lodepng: %s\n", lodepng_error_text(pngError));
        
        exit(EXIT_FAILURE);
    }
    
    // Load pixels from PNG
    pngError = lodepng_decode32(data, width, height, pngFile_d, pngFile_s);
    if (pngError)
    {
        printf("Failed to decode PNG; corrupted? (path \"%s\")\n", path);
        printf("Lodepng: %s\n", lodepng_error_text(pngError));
        free(pngFile_d); // Clean-up
        
        exit(EXIT_FAILURE);
    }
    
    // Calculate size of data
    *data_s = 4 * *width * *height;
    
    // Deallocate the PNG file data as we don't need it anymore
    free(pngFile_d);
}

// loadFile - Loads a file and stores it's data and size
void loadFile(uint8_t* path, uint8_t** data, size_t* size)
{
    FILE* inFile = fopen(path, "r");
    
    if (!inFile)
    {
        printf("Failed to load file %s\n", path);
        exit(EXIT_FAILURE);
    }
    
    fseek(inFile, 0, SEEK_END);
    *size = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);
    
    uint8_t* tmp = calloc(*size, sizeof(uint8_t));
    if (!tmp) printf("Could not malloc enough bytes for file %s\n", path);
    
    fread(tmp, sizeof(uint8_t), *size, inFile);
    *data = tmp;
    fclose(inFile);
}

int main(int argc, char **argv)
{
    // ARGUMENT PARSE //
    uint8_t* inPath   = 0; // Input file (encoding mode only)
    uint8_t* pngPath  = 0; // The PNG file used to store data in
    uint8_t* outPath  = 0; // File to write to
    uint8_t encode = DO_DECODE;
    
    // Parse arguments; this also does error checking
    parseArgs(argc, argv, &encode, &inPath, &pngPath, &outPath);
    
    // PNG LOAD //
    uint8_t* pngData_d = 0;            // PNG pixel data
    size_t  pngData_s;            // PNG pixel size
    uint32_t pngSize_x, pngSize_y; // PNG width/height
    
    // Load the PNG's pixel data
    loadPng(pngPath, &pngData_d, &pngData_s, &pngSize_x, &pngSize_y);
    
    printf("Loaded image %s (%ux%u)\n", pngPath, pngSize_x, pngSize_y);
    
    // Seed the xorshift PRNG
    pngpeek_seed(INITIAL_SEED);
    
    uint8_t* outData;
    size_t outSize;
    
    if (encode)
    {
        // ENCODING MODE //
        uint8_t* inFile_d = 0; // Input file data
        size_t  inFile_s;     // Input file size
        
        // Load file into memory
        loadFile(inPath, &inFile_d, &inFile_s);
        
        // Encoding mode writes directly into the image's data buffer
        pngpeek_encodeImage(pngData_d, pngData_s, inFile_d, inFile_s);
        
        // The message has been embedded; we need to write to file
        
        uint8_t* newPng_d;
        size_t newPng_s;
        
        lodepng_encode32(&newPng_d, &newPng_s, pngData_d, pngSize_x, pngSize_y);
        
        outData = newPng_d;
        outSize = newPng_s;
    } else {
        // DECODING MODE //
        
        // Decode the image's hidden message
        pngpeek_decodeImage(pngData_d, pngData_s, &outData, &outSize);
    }
    
    // Then write to file
    FILE* outFile = fopen(outPath, "w");

    if (!outFile)
    {
        printf("Invalid output path; exiting\n");
        exit(EXIT_FAILURE);
    }
    
    fwrite(outData, sizeof(uint8_t), outSize, outFile);
    fclose(outFile);
    
    return EXIT_SUCCESS;
}
