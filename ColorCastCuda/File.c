#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <windows.h>
#include "File.h"
#include "tinyfiledialogs.h"

// given a title and a message, sends a
// a popup window with given info
void sendPopup(const char* title, const char* msg) {
    tinyfd_messageBox(title, msg, "ok", "info", 0);
}

// given a title and a message, sends a
// a popup window with given info
// Icon is a yellow triangle with exclamation mark
void sendWarningPopup(const char* title, const char* msg) {
    tinyfd_messageBox(title, msg, "ok", "warning", 0);
}

// displays given message, if user presses yes, program exits
// if no, function returns
void shouldExit(char* msg) {
    int result = tinyfd_messageBox("", msg, "yesno", "question", 0);

    if (result == 1) {
        exit(0);
    }
}

// returns the path of a user selected a folder
char* getDir(const char* msg) {
    sendPopup("", msg);

    char workingDir[2048];
    // get the directory of the exe
    _getcwd(workingDir, sizeof(workingDir));
    // prompt user for path
    char* path = tinyfd_selectFolderDialog(NULL, workingDir);

    if (path == NULL) {
        // user hit cancel ask them if they want to exit the program
        shouldExit("You did not select a folder. Would you like to exit the program?");
        return getDir(msg);
    }

    char* result = _strdup(path);

    return result;
}

// get the power to be used in function dampenColors
double getPower() {
    // prompt user
    char* input = tinyfd_inputBox("", "Please enter a floating point number from .1 to 15."
        " .1 is completely gray,"
        " 15 is almost no change.", "");
    if (input == NULL) {
        // user hit cancel ask them if they want to exit program
        shouldExit("You did not enter anything. Would you like to exit the program?");
        return getPower();
    }

    // convert string answer to double
    // return 0 if there was an error
    char* eptr;
    double result = strtod(input, &eptr);
    // check to make sure double is in acceptable range
    if (result < .1 || result > 15) {
        shouldExit("Please enter a floating point number between .1 and 15. Would you like to exit the program?");
        return getPower();
    }

    return result;
}

char *toUpperCase(const char* lower) {
    char* res = _strdup(lower);

    while (*res) {
        *res = toupper((unsigned char)*res);
        res++;
    }

    return res -= strlen(lower);
}

// returns 1 if the filePath ends with the extension string
int isExtension(char* filePath, const char* extension) {
    char* upperExt = toUpperCase(extension);

    unsigned int len = strlen(filePath);
    unsigned int extLen = strlen(extension);

    // loop from back of string
    for (int i = 0; i < 3; i++) {
        if (filePath[len - i] != extension[extLen - i] && filePath[len - i] != upperExt[extLen - i]) {
            return 0;
        }
    }

    return 1;
}

// function adapted from: https://stackoverflow.com/questions/2314542/listing-directory-contents-using-c-and-windows
// returns the number of tif files in a given directory
int getNumImgInDir(const char* inputPath) {
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char sPath[2048];

    //Specify a file mask. *.* = We want everything!
    sprintf(sPath, "%s\\*.*", inputPath);

    if ((hFind = FindFirstFileA(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
        printf("Path not found: [%s]\n", inputPath);
        return 0;
    }

    int numImgs = 0;

    do {
        //Find first file will always return "."
        //    and ".." as the first two directories.
        if (strcmp(fdFile.cFileName, ".") != 0
            && strcmp(fdFile.cFileName, "..") != 0) {
            //Build up our file path using the passed in
            //  [inputPath] and the file/foldername we just found:
            sprintf(sPath, "%s\\%ls", inputPath, fdFile.cFileName);
            //sprintf(filePath, "%s\\%s", inputPath, fdFile.cFileName);

            //Is the entity a File or Folder?
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            }
            else {
                // if path is tif, jpg, or png add to numImgs
                if (isExtension(sPath, "tif") || isExtension(sPath, "tiff") || isExtension(sPath, "jpg") || isExtension(sPath, "png")) {
                    numImgs++;
                }
            }
        }
    } while (FindNextFile(hFind, &fdFile));

    FindClose(hFind);

    return numImgs;
}

// function adapted from: https://stackoverflow.com/questions/2314542/listing-directory-contents-using-c-and-windows
// sets imgPaths to contain the paths to all of the tif, jpg, and png files in the input directory
void setImagePaths(char** imgPaths, const char* inputPath) {
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char sPath[2048] ;

    //Specify a file mask. *.* = We want everything!
    sprintf(sPath, "%s\\*.*", inputPath);

    if ((hFind = FindFirstFileA(sPath, &fdFile)) == INVALID_HANDLE_VALUE) {
        printf("Path not found: [%s]\n", inputPath);
        return;
    }

    int imgIndex = 0;

    do {
        //Find first file will always return "."
        //    and ".." as the first two directories.
        if (strcmp(fdFile.cFileName, ".") != 0
            && strcmp(fdFile.cFileName, "..") != 0) {
            //Build up our file path using the passed in
            //  [inputPath] and the file/foldername we just found:
            sprintf(sPath, "%s\\%ls", inputPath, fdFile.cFileName);

            //Is the entity a File or Folder?
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            }
            else {
                // if path is tif, png, or jpg add to
                if (isExtension(sPath, "tif") || isExtension(sPath, "tiff") || isExtension(sPath, "jpg") || isExtension(sPath, "png")) {
                    imgPaths[imgIndex] = _strdup(sPath);
                    imgIndex++;
                }
            }
        }
    } while (FindNextFile(hFind, &fdFile));

    FindClose(hFind);
}

// given the input file and output directory, returns a string of the
// output file which will be in the output directory but have the
// input file name with the power as a prefix.
char* getOutputFilePath(char* inputFile, char* outputDir, double power) {
    char filename[400];
    char extension[10];
    _splitpath(_strdup(inputFile), NULL, NULL, filename, extension);

    char res[2048];
    sprintf(res, "%s\\%s.CC-%04.1f.%s", outputDir, filename, power, extension);

    return _strdup(res);
}

