#include <libgen.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "Image.h"
#include "Tiff.h"
#include "File.h"

const int NUM_CHANNELS = 3;

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
    double avg = (double) (c[0] + c[1] + c[2]) / NUM_CHANNELS;
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

// creates chunk of threads each responsible for 1 strip of the tif.
// returns once chunk of threads has finished processing pixels
void handleChunk(Tiff *tiff, double power, unsigned int chunkIndex, unsigned int numThreadsPerChunk) {
    int bytesPerChannel = tiff->bitsPerSample / 8;

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
        threadInfos[i].endPixel = tiff->bytesPerStrip[stripIndex] / (NUM_CHANNELS * bytesPerChannel);
        // create thread
        int error = pthread_create(&(pIds[i]), NULL, &processPixels, &threadInfos[i]);
        if (error != 0) {
            printf("\nThread can't be created :[%s]", strerror(error));
        }
    }

    // wait for all threads to finish before returning
    for (int i = 0; i < numThreadsPerChunk; i++) {
        pthread_join(pIds[i], NULL);
    }
}

// function that handles thread creation for multi striped threads
// each thread is responsible for one strip. Because there can be
// thousands of strips it is necessary to not create all of the threads
// at once. Instead this function creates numThreadsPerChunk threads at a
// time, waits for them to finish and then creates another numThreadsPerChunk
// threads again. These bunches of threads are refereed to as chunks
void handleMultiStrips(Tiff *tiff, double power, char *outputPath) {
    const int numThreadsPerChunk = 8;
    unsigned int numThreadChunks = tiff->numStrips / numThreadsPerChunk;
    // if numThreadsPerChunk does not divide evenly into numStrips
    // it is necessary to handle to the remaining few strips
    unsigned int remThreads = tiff->numStrips % numThreadsPerChunk;

    // loop through all of the chunks
    for (int chunkIndex = 0; chunkIndex < numThreadChunks; chunkIndex++) {
        handleChunk(tiff, power, chunkIndex, numThreadsPerChunk);
    }

    handleChunk(tiff, power, numThreadChunks, remThreads);

    // write tiff to output file
    writeTiff(tiff, outputPath);
}

// returns the length of the given file in bytes
// -1 if cannot get length
unsigned int getFileSize(char *filename) {
    struct stat discriptor;

    if (stat(filename, &discriptor) == 0) {
        return discriptor.st_size;
    }

    return -1;
}

// determines in a tiff is valid, if it processes the tif
// and saves it to the output file path
int handleTiff(char *imagePath, char *outputPath, double power) {
    unsigned int fileLen = getFileSize(imagePath);
    if (fileLen == -1) {
        printf("could not find file\n");
        return -1;
    }

    Tiff *tiff = openTiff(imagePath, fileLen);
    if (tiff == NULL) {
        return -1;
    }

    int result = 0;
    // isValidTiff will print the reason why the tiff is not valid
    if (isValidTiff(tiff)) {
        // handle tif according how many strips it has
        if (tiff->numStrips == 1) {
            handleSingleStrip(tiff, power, outputPath);
        } else {
            handleMultiStrips(tiff, power, outputPath);
        }
    } else {
        result = -1;
    }
    // free all data related to the tif
    free(tiff->data);
    free(tiff->entries);
    free(tiff->stripOffsets);
    free(tiff->bytesPerStrip);
    free(tiff);

    return result;
}

// handles any image that is not a tiff (jpg, png) normalizes all the
// pixels and then writes it to the outputPath
int handleImage(char *imagePath, char *outputPath, double power) {
    Image *img = getImage(imagePath);
    if (img == NULL) {
        return -1;
    }
    // loop through all the pixels in the image
    for (int row = 0; row < img->height; row++) {
        for (int col = 0; col < img->width; col++) {
            // pixels stored in 1d array, calculate start index of pixel
            int pixelStartIndex = (row * img->width + col) * NUM_CHANNELS;

            int *pixel = malloc(NUM_CHANNELS * sizeof(unsigned int));
            // get rgb values of pixel
            for (int i = 0; i < NUM_CHANNELS; i++) {
                pixel[i] = img->pix[pixelStartIndex + i];
            }

            normalize(pixel, power, 8);
            // write new pixel color back to pixel array
            for (int i = 0; i < NUM_CHANNELS; i++) {
                img->pix[pixelStartIndex + i] = pixel[i];
            }
        }
    }

    writeImage(img, outputPath);

    return 0;
}

// prints out timing info for program, notifies user if any images failed
// and lets the user know conversion has completed
void notifyUserAtEnd(double timeInSec, int numImg, int numFailedFiles, char errorCode[]) {
    // print out time information of program
    printf("\n------------------------------------------------------\n\n");
    printf("Time to complete: %.3f %s\n", timeInSec, "seconds");
    printf("Average time per image: %.3f\n", timeInSec / numImg);


    // notify user of failed images
    if (numFailedFiles > 0) {
        char msg[30];
        sprintf(msg, "Failed to convert %d images", numFailedFiles);
        sendWarningPopup("WARNING", msg);
        sendWarningPopup("WARNING", errorCode);
    }

    sendPopup("", "Conversion has completed!");
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
    int numImg = getNumImgInDir(inputPath);

    if (numImg == 0) {
        sendPopup("Error", "There are no supported images in the input directory you chose. Exiting program.");
        exit(0);
    }

    char errorCode[2048] = "";
    int numFailedFiles = 0;

    // allocate array for tif paths and set them
    char **imgPaths = malloc(numImg * sizeof(char *));
    setImagePaths(imgPaths, inputPath);
    // loop through each tif file and process it
    for (int i = 0; i < numImg; i++) {
        char *outputFile = getOutputFilePath(imgPaths[i], outputDirPath, power);
        printf("working on file: %s\n", imgPaths[i]);
        int result;
        if (isPNG(imgPaths[i]) || isJPG(imgPaths[i])) {
            result = handleImage(imgPaths[i], outputFile, power);
        } else {
            result = handleTiff(imgPaths[i], outputFile, power);
        }
        // if result of handleImage or handleTiff is -1, then it failed to process
        // the image, add to counter and add file name to list of failed images
        if (result == -1) {
            numFailedFiles++;

            strcat(errorCode, "\nFailed to convert: ");
            strcat(errorCode, basename(imgPaths[i]));
        }
    }

    double timeInSec = (double) (clock() - start) / 1000;
    notifyUserAtEnd(timeInSec, numImg, numFailedFiles, errorCode);

    return 0;
}


