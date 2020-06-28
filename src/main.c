#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Tiff.h"
#include "File.h"

// struct used for transferring info into threads that process each pixel
typedef struct {
    unsigned long startPixel;
    unsigned long endPixel;
    unsigned long pixelStartOffset;
    double power;
    Tiff *tiff;
} ThreadInfo;

// code adapted from: https://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
// maps a given value in one range into another range
double map(double input, double input_start, double input_end, double output_start, double output_end) {
    return output_start + ((output_end - output_start) / (input_end - input_start)) * (input - input_start);
}

// returns the dampened r,g, or b value of a color based on the avg value of
// the color and a grayness rating from 0 - 1. With 1 being true gray and 0
// being the opposite of grey
int dampenColor(int col, double avg, double grayness, double power) {
    double diff = fabs(col - avg);
    // the amount to move towards the avg value of the color
    // (how much to move towards a true gray)
    double change = diff * pow(grayness, power);

    // because diff is absolute valued it is necessary to check
    // if the original color is greater or less than the average
    // to determine if it is necessary to add or subtract change
    // from the original value
    if (col > avg) {
        return col - (int) round(change);
    }

    return col + (int) round(change);
}

// takes a color and returns a "normalized color" essentially moving color closer to true gray
// based on the original color and the value of pow
int *normalize(int *c, double power, int bitsPerSample) {
    // sum of the differences between r,g,b. Min of 0 (true gray) max of 510 (255 * 2, full red (255, 0, 0))
    double grayness = abs(c[0] - c[1]) + abs(c[0] - c[2]) + abs(c[2] - c[1]);

    int max = 65536 * 2;
    if (bitsPerSample == 8) {
        max = 255 * 2;
    }
    // maps grayness from range of [0, max] to [0, 1]
    grayness = map(grayness, 0, max, 0, 1);
    // reverses range. Now 1 is true gray and 0 is opposite of true gray
    grayness = 1 - grayness;

    // calculates the average rgb value of the color
    double avg = (double) (c[0] + c[1] + c[2]) / 3;
    // returns the nomalized color by "dampening" the rgb values individually
    c[0] = dampenColor(c[0], avg, grayness, power);
    c[1] = dampenColor(c[1], avg, grayness, power);
    c[2] = dampenColor(c[2], avg, grayness, power);

    return c;
}

// used for multithreading, function that actually normalizes
// pixels closer to true gray.
void *processPixels(void *info) {
    ThreadInfo *threadInfo = info;
    // loop through each assigned pixel and normalize it closer to true gray
    for (unsigned long i = threadInfo->startPixel; i < threadInfo->endPixel; i++) {
        // get rgb value for pixel
        int *pixel = getPixel(threadInfo->tiff, i, threadInfo->pixelStartOffset);
        normalize(pixel, threadInfo->power, threadInfo->tiff->bitsPerSample);
        // set pixel to new normalized value
        setPixel(threadInfo->tiff, pixel, i, threadInfo->pixelStartOffset);
    }

    return NULL;
}

// function that handles thread creation for single striped threads
// breaks image into numThreads sections, each section of pixels processed
// by a different thread
void handleSingleStrip(Tiff *tiff, double power, char *outputPath) {
    int numThreads = 8;
    unsigned long numPixels = getWidth(tiff) * getHeight(tiff);
    unsigned long pixelStartOffset = tiff->stripOffsets[0];

    ThreadInfo threadInfos[numThreads];
    pthread_t pIds[numThreads];
    // create each thread
    for (int threadNum = 0; threadNum < numThreads; threadNum++) {
        threadInfos[threadNum].tiff = tiff;
        threadInfos[threadNum].power = power;
        threadInfos[threadNum].pixelStartOffset = pixelStartOffset;
        threadInfos[threadNum].startPixel = threadNum * numPixels / numThreads;
        threadInfos[threadNum].endPixel = (threadNum + 1) * numPixels / numThreads;

        int error = pthread_create(&(pIds[threadNum]), NULL, &processPixels, &threadInfos[threadNum]);
        if (error != 0) {
            printf("\nThread can't be created :[%s]", strerror(error));
        }
    }

    // wait for all threads to finish converting
    for (int i = 0; i < numThreads; i++) {
        pthread_join(pIds[i], NULL);
    }
    // write tiff to output file
    writeTiff(tiff, outputPath);
}

// function that handles thread creation for multi striped threads
// each thread is responsible for one strip. Because there can be
// thousands of strips it is necessary to not create all of the threads
// at once. Instead this function creates numThreadsPerChunk threads at a
// time, waits for them to finish and then creates another numThreadsPerChunk
// threads again. These bunches of threads are refereed to as chunks
void handleMultiStripes(Tiff *tiff, double power, char *outputPath) {
    const int numThreadsPerChunk = 8;
    int bytesPerChannel = tiff->bitsPerSample / 8;
    unsigned int numThreadChunks = tiff->numStrips / numThreadsPerChunk;
    // if numThreadsPerChunk does not divide evenly into numStrips
    // it is necessary to handle to the remaining few strips
    unsigned int remThreadChunks = tiff->numStrips % numThreadsPerChunk;

    // loop through all of the chunks
    for (int chunkIndex = 0; chunkIndex < numThreadChunks; chunkIndex++) {
        ThreadInfo threadInfos[numThreadsPerChunk];
        pthread_t pIds[numThreadsPerChunk];
        // create each thread in the chunk of threads
        for (int i = 0; i < numThreadsPerChunk; i++) {
            unsigned int stripIndex = chunkIndex * numThreadsPerChunk + i;
            // setup ThreadInfo struct
            threadInfos[i].tiff = tiff;
            threadInfos[i].power = power;
            threadInfos[i].pixelStartOffset = tiff->stripOffsets[stripIndex];
            threadInfos[i].startPixel = 0;
            threadInfos[i].endPixel = tiff->bytesPerStrip[stripIndex] / (3 * bytesPerChannel);
            // create thread
            int error = pthread_create(&(pIds[i]), NULL, &processPixels, &threadInfos[i]);
            if (error != 0) {
                printf("\nThread can't be created :[%s]", strerror(error));
            }
        }

        // wait for all threads to finish before moving on to next chunk
        for (int i = 0; i < numThreadsPerChunk; i++) {
            pthread_join(pIds[i], NULL);
        }
    }

    // this next part of the function handles the remaining strips
    // if numStrips is not divisible by numThreadsPerChunk
    ThreadInfo threadInfos[remThreadChunks];
    pthread_t pIds[remThreadChunks];
    // remThreadChunks guaranteed to be less than numThreadsPerChunk
    for (int i = 0; i < remThreadChunks; i++) {
        unsigned int stripIndex = numThreadChunks * numThreadsPerChunk + i;
        threadInfos[i].tiff = tiff;
        threadInfos[i].power = power;
        threadInfos[i].pixelStartOffset = tiff->stripOffsets[stripIndex];
        threadInfos[i].startPixel = 0;
        threadInfos[i].endPixel = tiff->bytesPerStrip[stripIndex] / (3 * bytesPerChannel);

        int error = pthread_create(&(pIds[i]), NULL, &processPixels, &threadInfos[i]);
        if (error != 0) {
            printf("\nThread can't be created :[%s]", strerror(error));
        }
    }

    // wait for all threads to finish
    for (int i = 0; i < numThreadsPerChunk; i++) {
        pthread_join(pIds[i], NULL);
    }
    // write tiff to output file
    writeTiff(tiff, outputPath);
}

// determines in a tif is valid, if it processes the tif
// and saves it to the output file path
void handleImage(char *imagePath, char *outputPath, double power) {
    Tiff *tiff = openTiff(imagePath);
    if (tiff == NULL) {
        return;
    }
    // isValidTiff will print the reason why the tiff is not valid
    if (isValidTiff(tiff)) {
        // handle tif according how many strips it has
        if (tiff->numStrips == 1) {
            handleSingleStrip(tiff, power, outputPath);
        } else {
            handleMultiStripes(tiff, power, outputPath);
        }
    }
    // free all data related to the tif
    free(tiff->data);
    free(tiff->entries);
    free(tiff->stripOffsets);
    free(tiff->bytesPerStrip);
    free(tiff);
}

int main() {
    // get the paths for the input and output folders
    char *inputPath = getDir("Please select the folder of images you want to convert.");
    char *outputDirPath = getDir("Please select the folder where you want to save the output images.");
    // get the power from the user to specify how much the program should correct
    // to true gray
    double power = getPower();
    // start the clock
    clock_t start = clock();
    // get the number of tifs in the input directory
    int numTifs = getNumTifFilesInDir(inputPath);

    if (numTifs == 0) {
        sendPopup("Error", "There are no tif files in the input directory you chose. Exiting program.");
        exit(0);
    }

    // allocate array for tif paths and set them
    char **tifPaths = malloc(numTifs * sizeof(char *));
    setTifPaths(tifPaths, inputPath);
    // loop through each tif file and process it
    for (int i = 0; i < numTifs; i++) {
        char *outputFile = getOutputFilePath(tifPaths[i], outputDirPath, power);
        printf("working on file: %s\n", tifPaths[i]);
        handleImage(tifPaths[i], outputFile, power);
    }

    // print out time information of program
    double timeInSec = (double) (clock() - start) / 1000;
    printf("\n------------------------------------------------------\n\n");
    printf("Time to complete: %.3f %s\n", timeInSec, "seconds");
    printf("Average time per image: %.3f\n", timeInSec / numTifs);

    sendPopup("", "Conversion has completed!");

    return 0;
}


