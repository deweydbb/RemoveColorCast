# Remove Color Cast
Program to remove color casts from interior photos.
This program is meant as a tool for the photo editing process and is NOT designed to create finished photos.
This program supports .tif/.tiff files as they are a common file type when photo editing. In addition, the progam also supports .png and .jpg 

## Limitations
This program does not meet the requirements for a baseline tiff reader/writer as defined by the TIFF 6.0 specifications
The type of tifs this program supports is small. This program is also currently implemented to only run on windows. 

To be processed by the program, the tif must meet this requirements:
* Uncompressed (no internal compression)
* Contain only 1 image
* 3 channels per pixel (rgb)
* sRGB color space
* 8 or 16 bits per-channel

## CPU Branch
This branch of the repository has code that only executes on the cpu. This means that conversion of large tifs (8 and 16 bit) is relatively slow. This branch is meant for people who want use the program but do not have a cuda enabled nvidia graphics card. 

## What does the program do?
The program analyzes each pixel. The program moves the color of each pixel closer to true gray (rgb values all the same) depending on how grayness of the original pixel. This means that colors close to gray become true gray, and color that are not gray (red, orange, yellow, etc) remain relatively unchanged. 

The program will prompt the user to select and input folder of tifs they want to convert as well as an output folder where they want the results to be saved.

The user can dictate how much they want the program to move colors to true gray. They can enter floating point values in the range [0.1, 15]. Entering a value of 0.1 will make the entire image entirely grayscale, while 15 will barley have a perceptible change. Currently the scale is not linear, changing from 1 to 2 will have a much greater effect than changing from 14 to 15. 

## Examples

In this first example, notice how the walls and ceiling on the right side of the photo lose their red tint.
![](examples/example2.gif)

In this second example, notice how the desk and the wall behind it lose their yellow tint.
![](examples/example3.gif)

In this third example, notice how the cabinets on the right lose their blue tint. 
![](examples/example1.gif)

### Compiling for Windows
    gcc -o ColorCast.exe libs/*.c src/*.h src/*.c -pthread -lcomdlg32 -lole32 -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive  
