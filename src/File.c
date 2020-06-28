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

void shouldExit(char *msg) {
    int result = tinyfd_messageBox("", msg,"yesno", "question", 0);

    if (result == 1) {
        exit(0);
    }
}

char *getDir(char *msg) {
    sendPopup("", msg);

    char workingDir[2048];
    getcwd(workingDir, sizeof(workingDir));

    char *path = tinyfd_selectFolderDialog(NULL, workingDir);

    if (path == NULL) {
        shouldExit("You did not select a folder. Would you like to exit the program?");
        return getDir(msg);
    }

    char *result = strdup(path);
    free(path);

    return result;
}

double getPower() {
    char *input = tinyfd_inputBox("", "Please enter a floating point number from .1 to 15."
                                      " .1 is completely gray,"
                                      " 15 is almost no change.", "");

    if (input == NULL) {
        shouldExit("You did not enter anything. Would you like to exit the program?");
        return getPower();
    }

    char *eptr;
    double result = strtod(input, &eptr);

    if (result < .1 || result > 15) {
        shouldExit("Please enter a floating point number between .1 and 15. Would you like to exit the program?");
        return getPower();
    }

    return result;
}

int isTif(char *filePath) {
    unsigned int len = strlen(filePath);
    const char *TIF = "tif";
    const char *TIF_UPPER = "TIF";

    for (int i = 0; i < 3; i++) {
        if (filePath[len - i] != TIF[3 - i] && filePath[len - i] != TIF_UPPER[3 - i]) {
            return 0;
        }
    }

    return 1;
}

int isTiff(char *filePath) {
    unsigned int len = strlen(filePath);
    const char *TIF = "tiff";

    for (int i = 0; i < 4; i++) {
        if (filePath[len - i] != TIF[4 - i]) {
            return 0;
        }
    }

    return 1;
}

// function adapted from: https://stackoverflow.com/questions/2314542/listing-directory-contents-using-c-and-windows
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

char *getOutputFilePath(char *inputFile, char *outputDir, double power) {
    char *filename = basename(strdup(inputFile));

    char *res = strdup(outputDir);

    strcat(res, "\\");

    char prefix[1024];
    sprintf(prefix, "%.2f", power);
    strcat(prefix, "_");
    strcat(prefix, filename);

    return strcat(res, prefix);
}
