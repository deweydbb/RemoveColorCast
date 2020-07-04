#include <cuda_runtime.h>
#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Process.cuh"
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

__global__ void processPixel(unsigned char* data, unsigned int offset, double power, int bytesPerChannel, int isLittle, unsigned int max) {
    unsigned int pixelNum = threadIdx.x + blockIdx.x * blockDim.x;
    unsigned int startPtr = offset + (pixelNum * 3 * bytesPerChannel);

    if (startPtr < max) {
        int red = data[startPtr];
        int green = data[startPtr + 1];
        int blue = data[startPtr + 2];
        if (bytesPerChannel == 2 && isLittle) {
            red = data[startPtr];
            red += data[startPtr + 1] << 8;
            green = data[startPtr + 2];
            green += data[startPtr + 3] << 8;
            blue = data[startPtr + 4];
            blue += data[startPtr + 5] << 8;
        } 
        else if (bytesPerChannel == 2) {
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
       
        if (bytesPerChannel == 1) {
            data[startPtr] = red;
            data[startPtr + bytesPerChannel] = green;
            data[startPtr + (bytesPerChannel * 2)] = blue;
        }
        else if (isLittle) {
            data[startPtr] = red;
            data[startPtr + 1] = red >> 8;
            data[startPtr + 2] = green;
            data[startPtr + 3] = green >> 8;
            data[startPtr + 4] = blue;
            data[startPtr + 5] = blue >> 8;
        }
        else {
            data[startPtr] = red >> 8;
            data[startPtr + 1] = red;
            data[startPtr + 2] = green >> 8;
            data[startPtr + 3] = green;
            data[startPtr + 4] = blue >> 8;
            data[startPtr + 5] = blue;
        }
        
    }
}

int handleImage(char* imagePath, char* outputPath, double power) {
    Image* img = getImage(imagePath);
    const int NUM_CHANNELS = 3;
    if (img == NULL) {
        return -1;
    }

    unsigned char* d_pix;
    unsigned int numPix = img->width * img->height;

    cudaError_t err = cudaMalloc(&d_pix, numPix * NUM_CHANNELS * sizeof(char));
    if (err != cudaSuccess) {
        printf("Error on malloc %s\n", cudaGetErrorString(err));
        return -1;
    }

    err = cudaMemcpy(d_pix, img->pix, numPix * NUM_CHANNELS * sizeof(char), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Error on memcopy htd %s\n", cudaGetErrorString(err));
        return -1;
    }

    int threadsPerBlock = 256;
    int blocksPerGrid = (numPix + threadsPerBlock - 1) / threadsPerBlock;

    processPixel <<<blocksPerGrid, threadsPerBlock>>> (d_pix, 0, power, 1, 1, numPix * 3);
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Error on process pixels %s\n", cudaGetErrorString(err));
        return -1;
    }


    err = cudaMemcpy(img->pix, d_pix, numPix * NUM_CHANNELS * sizeof(char), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Error on memcopy dth %s\n", cudaGetErrorString(err));
        return -1;
    }

    err = cudaFree(d_pix);
    if (err != cudaSuccess) {
        printf("Error on free in main %s\n", cudaGetErrorString(err));
        return -1;
    }

    writeImage(img, outputPath);

    return 0;
}

int handleSingleStrip(Tiff* tiff, double power, char* outputPath) {
    unsigned long numPixels = getWidth(tiff) * getHeight(tiff);
    unsigned long pixelStartOffset = tiff->stripOffsets[0];
    unsigned int numBytes = tiff->bytesPerStrip[0];
    unsigned char* d_pix;

    clock_t start = clock();

    cudaError_t err = cudaMalloc(&d_pix, tiff->bytesPerStrip[0] * sizeof(char));
    if (err != cudaSuccess) {
        printf("Error on malloc %s\n", cudaGetErrorString(err));
        return -1;
    }
    
    err = cudaMemcpy(d_pix, tiff->data + pixelStartOffset, numBytes, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Error on memcopy htd %s\n", cudaGetErrorString(err));
        return -1;
    }

    int threadsPerBlock = 256;
    int blocksPerGrid = (numPixels + threadsPerBlock - 1) / threadsPerBlock;
    int bytesPerChannel = tiff->bitsPerSample / 8;
    int isLittle = tiff->isLittle;

    processPixel <<<blocksPerGrid, threadsPerBlock>>> (d_pix, 0, power, bytesPerChannel, isLittle, numBytes);

    err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Error on process pixels %s\n", cudaGetErrorString(err));
        return -1;
    }

    err = cudaMemcpy(tiff->data + pixelStartOffset, d_pix, numBytes * sizeof(char), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Error on memcopy dth %s\n", cudaGetErrorString(err));
        return -1;
    }

    err = cudaFree(d_pix);
    if (err != cudaSuccess) {
        printf("Error on free in main %s\n", cudaGetErrorString(err));
        return -1;
    }

    //printf("total process data time: %.f\n", (double)(clock() - start));

    start = clock();
    writeTiff(tiff, outputPath);
    //printf("write time: %.3f\n", (double)(clock() - start) / 1000);
    
    return 0;

}

int handleMultiStrips(Tiff* tiff, double power, char* outputPath) {
    unsigned char* d_pix;

    clock_t start = clock();

    cudaError_t err = cudaMalloc(&d_pix, tiff->dataLen);
    if (err != cudaSuccess) {
        printf("Error on malloc %s\n", cudaGetErrorString(err));
        return -1;
    }

    err = cudaMemcpy(d_pix, tiff->data, tiff->dataLen, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        printf("Error on memcopy htd %s\n", cudaGetErrorString(err));
        return -1;
    }

    int isLittle = tiff->isLittle;
    int bytesPerChannel = tiff->bitsPerSample / 8;
    int threadsPerBlock = 256;

    for (int i = 0; i < tiff->numStrips; i++) {
        int numPixelsInStrip = tiff->bytesPerStrip[i] / (3 * bytesPerChannel);
        int blocksPerGrid = (numPixelsInStrip + threadsPerBlock - 1) / threadsPerBlock;
        unsigned int max = tiff->stripOffsets[i] + tiff->bytesPerStrip[i];
        processPixel << <blocksPerGrid, threadsPerBlock >> > (d_pix, tiff->stripOffsets[i], power, bytesPerChannel, isLittle, max);

        err = cudaGetLastError();
        if (err != cudaSuccess) {
            printf("Error on process pixels %s\n", cudaGetErrorString(err));
            return -1;
        }
    }

    err = cudaMemcpy(tiff->data, d_pix, tiff->dataLen, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        printf("Error on memcopy dth %s\n", cudaGetErrorString(err));
        return -1;
    }

    err = cudaFree(d_pix);
    if (err != cudaSuccess) {
        printf("Error on free in main %s\n", cudaGetErrorString(err));
        return -1;
    }

    //printf("total process data time: %.f\n", (double)(clock() - start));

    start = clock();
    writeTiff(tiff, outputPath);
    //printf("write time: %.3f\n", (double)(clock() - start) / 1000);

    return 0;
}

