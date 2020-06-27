#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "Tiff.h"

double power = .5;

double map(double input, double input_start, double input_end, double output_start, double output_end){
    return output_start + ((output_end - output_start) / (input_end - input_start)) * (input - input_start);
}


// returns the dampened r,g, or b value of a color based on the avg value of
// the color and a grayness rating from 0 - 1. With 1 being true gray and 0
// being the opposite of grey
int dampenColor(int col, double avg, double grayness) {
    double diff = fabs(col - avg);
    // the amount to move towards the avg value of the color
    // (how much to move towards a true gray)
    double change =  diff * pow(grayness, power);

    // because diff is aboslute valued it is neccecarry to check
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
int *normalize(int *c) {
    // sum of the differences between r,g,b. Min of 0 (true gray) max of 510 (255 * 2, full red (255, 0, 0))
    double grayness = abs(c[0] - c[1]) + abs(c[0] - c[2]) + abs(c[2] - c[1]);
    // maps grayness from range of [0, 255] to [0, 1]
    grayness = map(grayness, 0, 65536 * 2, 0, 1);
    // reverses range. Now 1 is true gray and 0 is opposite of true gray
    grayness = 1 - grayness;

    // calculates the average rgb value of the color
    double avg = (double) (c[0] + c[1] + c[2]) / 3;
    // returns the nomalized color by "dampening" the rgb values individually
    c[0] = dampenColor(c[0], avg, grayness);
    c[1] = dampenColor(c[1], avg, grayness);
    c[2] = dampenColor(c[2], avg, grayness);

    return c;
}

int main() {
    Tiff *tiff = openTiff("../Images/test.tif");

    if (isValidTiff(tiff)) {
        unsigned long numPixels = getWidth(tiff) * getHeight(tiff);
        unsigned long pixelStartOffset = getPixelStartOffset(tiff);

        for (unsigned long i = 0; i < numPixels; i++) {
            int *pixel = getPixel(tiff, i, pixelStartOffset);
            setPixel(tiff, normalize(pixel), i, pixelStartOffset);
        }

        writeTiff(tiff, "../Images/output.tif");
    }

    return 0;
}


