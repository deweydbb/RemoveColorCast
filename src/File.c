#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
#include "File.h"
#include "../libs/tinyfiledialogs.h"

// given a title and a message, sends a
// a popup window with given info
void sendPopup(char *title, char *msg) {
    tinyfd_messageBox(title, msg,"ok", "info", 0);
}

// displays given message, if user presses yes, program exits
// if no, function returns
void shouldExit(char *msg) {
    int result = tinyfd_messageBox("", msg,"yesno", "question", 0);

    if (result == 1) {
        exit(0);
    }
}

// returns the path of a user selected a folder
char *getDir(char *msg) {
    sendPopup("", msg);

    char workingDir[2048];
    // get the directory of the exe
    getcwd(workingDir, sizeof(workingDir));
    // prompt user for path
    char *path = tinyfd_selectFolderDialog(NULL, workingDir);

    if (path == NULL) {
        // user hit cancel ask them if they want to exit the program
        shouldExit("You did not select a folder. Would you like to exit the program?");
        return getDir(msg);
    }

    char *result = strdup(path);
    free(path);

    return result;
}

// get the power to be used in function dampenColors
double getPower() {
    // prompt user
    char *input = tinyfd_inputBox("", "Please enter a floating point number from .1 to 15."
                                      " .1 is completely gray,"
                                      " 15 is almost no change.", "");
    if (input == NULL) {
        // user hit cancel ask them if they want to exit program
        shouldExit("You did not enter anything. Would you like to exit the program?");
        return getPower();
    }

    // convert string answer to double
    // return 0 if there was an error
    char *eptr;
    double result = strtod(input, &eptr);
    // check to make sure double is in acceptable range
    if (result < .1 || result > 15) {
        shouldExit("Please enter a floating point number between .1 and 15. Would you like to exit the program?");
        return getPower();
    }

    return result;
}

// determine if a file ends in a .tif or .TIF extension
int isTif(char *filePath) {
    unsigned int len = strlen(filePath);
    const char *TIF = "tif";
    const char *TIF_UPPER = "TIF";
    // loop from back of string
    for (int i = 0; i < 3; i++) {
        if (filePath[len - i] != TIF[3 - i] && filePath[len - i] != TIF_UPPER[3 - i]) {
            return 0;
        }
    }

    return 1;
}

// determine if file ends in a .tiff or .TIFF extension
int isTiff(char *filePath) {
    unsigned int len = strlen(filePath);
    const char *TIF = "tiff";
    const char *TIF_UPPER = "TIFF";

    for (int i = 0; i < 4; i++) {
        if (filePath[len - i] != TIF[4 - i] && filePath[len - i] != TIF_UPPER[4 - i]) {
            return 0;
        }
    }

    return 1;
}

// function adapted from: https://stackoverflow.com/questions/2314542/listing-directory-contents-using-c-and-windows
// returns the number of tif files in a given directory
int getNumTifFilesInDir(const char *inputPath) {
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char sPath[2048];

    //Specify a file mask. *.* = We want everything!
    sprintf(sPath, "%s\\*.*", inputPath);

    if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
        printf("Path not found: [%s]\n", inputPath);
        return 0;
    }

    int numTifs = 0;

    do {
        //Find first file will always return "."
        //    and ".." as the first two directories.
        if (strcmp(fdFile.cFileName, ".") != 0
            && strcmp(fdFile.cFileName, "..") != 0) {
            //Build up our file path using the passed in
            //  [inputPath] and the file/foldername we just found:
            sprintf(sPath, "%s\\%s", inputPath, fdFile.cFileName);

            //Is the entity a File or Folder?
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            } else {
                // if path is tif, add to numTifs
                if (isTif(sPath) || isTiff(sPath)) {
                    numTifs++;
                }
            }
        }
    } while (FindNextFile(hFind, &fdFile));

    FindClose(hFind);

    return numTifs;
}

// function adapted from: https://stackoverflow.com/questions/2314542/listing-directory-contents-using-c-and-windows
// sets tifPaths to contain the paths to all of the tif files in the input directory
void setTifPaths(char **tifPaths, const char *inputPath) {
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char sPath[2048];

    //Specify a file mask. *.* = We want everything!
    sprintf(sPath, "%s\\*.*", inputPath);

    if ((hFind = FindFirstFile(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
        printf("Path not found: [%s]\n", inputPath);
        return;
    }

    int tifIndex = 0;

    do {
        //Find first file will always return "."
        //    and ".." as the first two directories.
        if (strcmp(fdFile.cFileName, ".") != 0
            && strcmp(fdFile.cFileName, "..") != 0) {
            //Build up our file path using the passed in
            //  [inputPath] and the file/foldername we just found:
            sprintf(sPath, "%s\\%s", inputPath, fdFile.cFileName);

            //Is the entity a File or Folder?
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            } else {
                // if path is tif, add to numTifs
                if (isTif(sPath) || isTiff(sPath)) {
                    tifPaths[tifIndex] = strdup(sPath);
                    tifIndex++;
                }
            }
        }
    } while (FindNextFile(hFind, &fdFile));

    FindClose(hFind);
}

// given the input file and output directory, returns a string of the
// output file which will be in the output directory but have the
// input file name with the power as a prefix.
char *getOutputFilePath(char *inputFile, char *outputDir, double power) {
    char *filename = basename(strdup(inputFile));

    char res[2048];
    strcpy(res, outputDir);

    strcat(res, "\\");

    char prefix[2048];
    sprintf(prefix, "%.2f_%s", power, filename);
    strcat(res, prefix);

    return strdup(res);
}
