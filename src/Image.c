#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "File.h"
#include "Image.h"
#include "../libs/stb_image.h"
#include "../libs/stb_image_write.h"

// returns a struct that contains an array of pixels, width and height of an image
// in rgb. Exits program if unable to load image.
Image *getImage(char *path) {
    int width, height, channels;
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 3);
    // failed to load image
    if (pixels == NULL) {
        printf("Failed to load image\n");
        return NULL;
    }
    // create image struct
    Image *img = malloc(sizeof(Image));
    img->width = width;
    img->height = height;
    img->pix = pixels;

    return img;
}

// writes the given image to the given outputPath
void writeImage(Image *img, char *outputPath) {
    if (isExtension(outputPath, "jpg")) {
        stbi_write_jpg(outputPath, img->width, img->height, 3, img->pix, 100);
    } else {
        stbi_write_png(outputPath, img->width, img->height, 3, img->pix, img->width * 3);
    }

    // free up image
    free(img->pix);
    free(img);
}


