#ifndef COLORCAST_IMAGE_H
#define COLORCAST_IMAGE_H

typedef struct {
    int width;
    int height;
    unsigned char* pix;
} Image;

// loads and returns a pointer to the image struct
// based on the given path
Image* getImage(char* path);

// writes the given image to the given outputPath
void writeImage(Image* image, char* outputPath);

#endif //COLORCAST_IMAGE_H
