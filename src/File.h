#ifndef COLORCAST_FILE_H
#define COLORCAST_FILE_H

char *getDir(char *msg);

double getPower();

int getNumTifFilesInDir(const char *inputPath);

void setTifPaths(char **tifPaths, const char *inputPath);

char *getOutputFilePath(char *inputFile, char *outputDir, double power);


#endif //COLORCAST_FILE_H
