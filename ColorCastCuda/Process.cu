#include <cuda_runtime.h>
#include <cuda.h>
#include <stdio.h>
#include "device_launch_parameters.h"
#include "cudart_platform.h"


extern "C" {
    #include "Image.h"
    #include "Tiff.h"
}

// code adapted from: https://stackoverflow.com/questions/5731863/mapping-a-numeric-range-onto-another
// maps a given value in one range into another range
__device__ double mapDouble(double input, double input_start, double input_end, double output_start, double output_end) {
    return output_start + ((output_end - output_start) / (input_end - input_start)) * (input - input_start);
}

// super fancy custom function because: double abs(double) has multiple definitions
// so this is my HIGH tech work around
__device__ double absoluteVal(double val) {
    if (val < 0) {
        return val * -1;
    }

    return val;
}

// returns the dampened r,g, or b value of a color based on the avg value of
// the color and a grayness rating from 0 - 1. With 1 being true gray and 0
// being the opposite of grey
__device__ int dampenColor(int col, double avg, double grayness, double power) {
    double diff = absoluteVal(col - avg);
    // the amount to move towards the avg value of the color
    // (how much to move towards a true gray)
    double change = diff * pow(grayness, power);

    // because diff is absolute valued it is necessary to check
    // if the original color is greater or less than the average
    // to determine if it is necessary to add or subtract change
    // from the original value
    if (col > avg) {
        return col - rint(change);
    }

    return col + (int) rint(change);
}

// thread responsible for processing one pixel of the image
// bytesPerChannel specifies whether file store rgb values in 8 or 16 bit integers.
// max is the pointer 1 after the end of the pixel data. Becaue each thread block has a fixed number of threads
// there is one block that will have excces threads. It is necessary to make sure these threads do nothing. 
__global__ void processPixel(unsigned char* data, unsigned int offset, double power, int bytesPerChannel, int isLittle, unsigned int max) {
    unsigned int pixelNum = threadIdx.x + blockIdx.x * blockDim.x;
    unsigned int startPtr = offset + (pixelNum * 3 * bytesPerChannel);
    // check to make sure startPtr is a valid pointer to pixel data
    if (startPtr < max) {
        // values hardcoded because having local variables is faster than calling malloc on the gpu
        // to create an array
        int red = 0;
        int green = 0;
        int blue = 0;
        
        if (bytesPerChannel == 1) {
            red = data[startPtr];
            green = data[startPtr + 1];
            blue = data[startPtr + 2];
        } else if (isLittle) {
            red = data[startPtr];
            red += data[startPtr + 1] << 8;
            green = data[startPtr + 2];
            green += data[startPtr + 3] << 8;
            blue = data[startPtr + 4];
            blue += data[startPtr + 5] << 8;
        } else {
            red = data[startPtr] << 8;
            red += data[startPtr + 1];
            green = data[startPtr + 2] << 8;
            green += data[startPtr + 3];
            blue = data[startPtr + 4] << 8;
            blue += data[startPtr + 5];
        }
        
        double grayness = abs(red - green) + abs(red - blue) + abs(blue - green);

        int maxRange = 65536 * 2;
        if (bytesPerChannel == 1) {
            maxRange = 255 * 2;
        }
        
        // maps grayness from range of [0, maxRange] to [0, 1]
        grayness = mapDouble(grayness, 0, maxRange, 0, 1);
        // reverses range. Now 1 is true gray and 0 is opposite of true gray
        grayness = 1 - grayness;
        
        // calculates the average rgb value of the color
        double avg = (double) (red + green + blue) / 3;
        // returns the nomalized color by "dampening" the rgb values individually
        red = dampenColor(red, avg, grayness, power);
        green = dampenColor(green, avg, grayness, power);
        blue = dampenColor(blue, avg, grayness, power);
        // set processed rgb values back out to gpu global memory
        if (bytesPerChannel == 1) {
            data[startPtr] = red;
            data[startPtr + bytesPerChannel] = green;
            data[startPtr + (bytesPerChannel * 2)] = blue;
        }
        else if (isLittle) {
            // little endian 16 bit
            data[startPtr] = red;
            data[startPtr + 1] = red >> 8;
            data[startPtr + 2] = green;
            data[startPtr + 3] = green >> 8;
            data[startPtr + 4] = blue;
            data[startPtr + 5] = blue >> 8;
        }
        else {
            // big endian 16 bit
            data[startPtr] = red >> 8;
            data[startPtr + 1] = red;
            data[startPtr + 2] = green >> 8;
            data[startPtr + 3] = green;
            data[startPtr + 4] = blue >> 8;
            data[startPtr + 5] = blue;
        }
        
    }
}

// processes any image on the gpu that is not a tiff
// copies over pixel data to gpu and creates a thread for every pixel
int handleImage(char* imagePath, char* outputPath, double power) {
    // load in the image
    Image* img = getImage(imagePath);
    const int NUM_CHANNELS = 3;
    if (img == NULL) {
        return -1;
    }

    unsigned char* d_pix;
    unsigned int numPix = img->width * img->height;
    // allocate memory on the gpu
    cudaError_t err = cudaMalloc(&d_pix, numPix * NUM_CHANNELS * sizeof(char));
    if (err != cudaSuccess) {
        printf("Error on malloc %s\n", cudaGetErrorString(err));
        return -1;
    }
    // copy over pixel data to gpu 
    err = cudaMemcpy(d_pix, img->pix, numPix * NUM_CHANNELS * sizeof(char), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Error on memcopy htd %s\n", cudaGetErrorString(err));
        return -1;
    }

    int threadsPerBlock = 256;
    int blocksPerGrid = (numPix + threadsPerBlock - 1) / threadsPerBlock;
    // create threads on gpu to process each individual pixel
    processPixel <<<blocksPerGrid, threadsPerBlock>>> (d_pix, 0, power, 1, 1, numPix * 3);
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Error on process pixels %s\n", cudaGetErrorString(err));
        return -1;
    }
    // copy over pixel data from gpu to cpu
    err = cudaMemcpy(img->pix, d_pix, numPix * NUM_CHANNELS * sizeof(char), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Error on memcopy dth %s\n", cudaGetErrorString(err));
        return -1;
    }
    // free memory on the gpu
    err = cudaFree(d_pix);
    if (err != cudaSuccess) {
        printf("Error on free in main %s\n", cudaGetErrorString(err));
        return -1;
    }
    // write image to output file
    writeImage(img, outputPath);
    // return 0 indicating success
    return 0;
}

// handles a single strip tiff. Copies over just the pixel data to the gpu
// creates gpu thread for each pixel
int handleSingleStrip(Tiff* tiff, double power, char* outputPath) {
    unsigned long numPixels = getWidth(tiff) * getHeight(tiff);
    unsigned long pixelStartOffset = tiff->stripOffsets[0];
    unsigned int numBytes = tiff->bytesPerStrip[0];
    unsigned char* d_pix;
    // allocate space on gpu for pixel data of tiff
    cudaError_t err = cudaMalloc(&d_pix, tiff->bytesPerStrip[0] * sizeof(char));
    if (err != cudaSuccess) {
        printf("Error on malloc %s\n", cudaGetErrorString(err));
        return -1;
    }
    // copy over pixel data to gpu
    err = cudaMemcpy(d_pix, tiff->data + pixelStartOffset, numBytes, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Error on memcopy htd %s\n", cudaGetErrorString(err));
        return -1;
    }

    int threadsPerBlock = 256;
    // creates enough blockes so there is one thread per pixel
    int blocksPerGrid = (numPixels + threadsPerBlock - 1) / threadsPerBlock;
    int bytesPerChannel = tiff->bitsPerSample / 8;
    int isLittle = tiff->isLittle;
    // create threads on gpu
    processPixel <<<blocksPerGrid, threadsPerBlock>>> (d_pix, 0, power, bytesPerChannel, isLittle, numBytes);
    // check for error on threads in gpu
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Error on process pixels %s\n", cudaGetErrorString(err));
        return -1;
    }
    // copy processed pixel data from gpu to cpu
    err = cudaMemcpy(tiff->data + pixelStartOffset, d_pix, numBytes * sizeof(char), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Error on memcopy dth %s\n", cudaGetErrorString(err));
        return -1;
    }
    // free memory on gpu
    err = cudaFree(d_pix);
    if (err != cudaSuccess) {
        printf("Error on free in main %s\n", cudaGetErrorString(err));
        return -1;
    }
    // write tiff to output file
    writeTiff(tiff, outputPath);
    // return 0 indicating success
    return 0;
}

// the handeling of multi stripped tiffs is handled separately because there is no guarantee that the strips will be 
// placed continuously throughout the file so it faster to copy the entire file all at once to the gpu than copy over
// each strip. This not does not make sense for singely stripped tiffs, where the pixels are guaranteed to be stored 
// continuously in the file. creates thread for each pixel.
int handleMultiStrips(Tiff* tiff, double power, char* outputPath) {
    unsigned char* d_pix;

    // malloc enough gpu memory for the entire tiff file
    cudaError_t err = cudaMalloc(&d_pix, tiff->dataLen);
    if (err != cudaSuccess) {
        printf("Error on malloc %s\n", cudaGetErrorString(err));
        return -1;
    }
    // copy over entire tiff file to gpu
    err = cudaMemcpy(d_pix, tiff->data, tiff->dataLen, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Error on memcopy htd %s\n", cudaGetErrorString(err));
        return -1;
    }
    
    int isLittle = tiff->isLittle;
    int bytesPerChannel = tiff->bitsPerSample / 8;
    int threadsPerBlock = 256;
    // loop through each strip of the tiff 
    for (int i = 0; i < tiff->numStrips; i++) {
        int numPixelsInStrip = tiff->bytesPerStrip[i] / (3 * bytesPerChannel);
        int blocksPerGrid = (numPixelsInStrip + threadsPerBlock - 1) / threadsPerBlock;
        // max pointer value of the strip
        unsigned int max = tiff->stripOffsets[i] + tiff->bytesPerStrip[i];
        // processPixel is an async call so the next strip can be setup relatively quickly
        processPixel << <blocksPerGrid, threadsPerBlock >> > (d_pix, tiff->stripOffsets[i], power, bytesPerChannel, isLittle, max);
        // check for error while processing pixels
        err = cudaGetLastError();
        if (err != cudaSuccess) {
            printf("Error on process pixels %s\n", cudaGetErrorString(err));
            return -1;
        }
    }
    // copy tiff file from gpu to cpu
    err = cudaMemcpy(tiff->data, d_pix, tiff->dataLen, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Error on memcopy dth %s\n", cudaGetErrorString(err));
        return -1;
    }
    // free gpu memory
    err = cudaFree(d_pix);
    if (err != cudaSuccess) {
        printf("Error on free in main %s\n", cudaGetErrorString(err));
        return -1;
    }
    // write the tiff to the output file
    writeTiff(tiff, outputPath);
    // return 0 indicating success
    return 0;
}

