#ifndef COLORCAST_FILE_H
#define COLORCAST_FILE_H

// given a title and a message, sends a
// a popup window with given info
void sendPopup(char *title, char *msg);

// returns the path of a user selected a folder
char *getDir(char *msg);

// get the power to be used in function dampenColors
double getPower();

// returns the number of tif files in a given directory
int getNumTifFilesInDir(const char *inputPath);

// sets tifPaths to contain the paths to all of the tif files in the input directory
void setTifPaths(char **tifPaths, const char *inputPath);

// given the input file and output directory, returns a string of the
// output file which will be in the output directory but have the
// input file name with the power as a prefix.
char *getOutputFilePath(char *inputFile, char *outputDir, double power);


#endif //COLORCAST_FILE_H
