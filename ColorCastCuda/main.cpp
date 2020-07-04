#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

extern "C" {
	#include "Image.h"
	#include "Tiff.h"
	#include "File.h"
}

extern "C" const int NUM_CHANNELS = 3;
// functions in Process.cu that process images on the gpu and write out to output file
extern int handleImage(char* imagePath, char* outputPath, double power);
extern int handleSingleStrip(Tiff* tiff, double power, char* outputPath);
extern int handleMultiStrips(Tiff* tiff, double power, char* outputPath);

// returns the length of the given file in bytes
// -1 if cannot get length
unsigned int getFileSize(char* filename) {
	struct stat discriptor;

	if (stat(filename, &discriptor) == 0) {
		return discriptor.st_size;
	}

	return -1;
}

// determines in a tiff is valid, if it processes the tif
// and saves it to the output file path
// return 0 for success, -1 for failure
int handleTiff(char* imagePath, char* outputPath, double power) {
	unsigned int fileLen = getFileSize(imagePath);
	if (fileLen == -1) {
		printf("could not find file\n");
		return -1;
	}

	Tiff* tiff = openTiff(imagePath, fileLen);
	if (tiff == NULL) {
		return -1;
	}

	int result = 0;
	// isValidTiff will print the reason why the tiff is not valid
	if (isValidTiff(tiff)) {
		// handle tif according how many strips it has
		if (tiff->numStrips == 1) {
			result = handleSingleStrip(tiff, power, outputPath);
		}
		else {
			result = handleMultiStrips(tiff, power, outputPath);
		}
	}
	else {
		result = -1;
	}
	// free all data related to the tif
	free(tiff->data);
	free(tiff->entries);
	free(tiff->stripOffsets);
	free(tiff->bytesPerStrip);
	free(tiff);
	
	return result;
}

// prints out timing info for program, notifies user if any images failed
// and lets the user know conversion has completed
void notifyUserAtEnd(double timeInSec, int numImg, int numFailedFiles, char errorCode[]) {
	// print out time information of program
	printf("\n------------------------------------------------------\n\n");
	printf("Time to complete: %.3f %s\n", timeInSec, "seconds");
	printf("Average time per image: %.3f\n", timeInSec / numImg);


	// notify user of failed images
	if (numFailedFiles > 0) {
		char msg[30];
		sprintf(msg, "Failed to convert %d images", numFailedFiles);
		sendWarningPopup("WARNING", msg);
		sendWarningPopup("WARNING", errorCode);
	}

	sendPopup("", "Conversion has completed!");
}

int main() {
	// get the paths for the input and output folders
	char* inputPath = getDir("Please select the folder of images you want to convert.");
	char* outputDirPath = getDir("Please select the folder where you want to save the output images.");
	// get the power from the user to specify how much the program should correct
	// to true gray
	double power = getPower();
	// start the clock
	clock_t start = clock();
	// get the number of tifs in the input directory
	int numImg = getNumImgInDir(_strdup(inputPath));

	if (numImg == 0) {
		sendPopup("Error", "There are no supported images in the input directory you chose. Exiting program.");
		exit(0);
	}

	char errorCode[2048] = "";
	int numFailedFiles = 0;

	// allocate array for tif paths and set them
	char** imgPaths = (char**) malloc(numImg * sizeof(char *));
	setImagePaths(imgPaths, inputPath);
	// loop through each tif file and process it
	for (int i = 0; i < numImg; i++) {
		char* outputFile = getOutputFilePath(imgPaths[i], outputDirPath, power);
		printf("working on file: %s\n", imgPaths[i]);
		int result;
		if (isExtension(imgPaths[i], "jpg") || isExtension(imgPaths[i], "png")) {
			result = handleImage(imgPaths[i], outputFile, power);
		}
		else {
			result = handleTiff(imgPaths[i], outputFile, power);
		}

		free(outputFile);
		// if result of handleImage or handleTiff is -1, then it failed to process
		// the image, add to counter and add file name to list of failed images
		if (result == -1) {
			numFailedFiles++;
			char fileName[400];
			char ext[10];
			_splitpath(imgPaths[i], NULL, NULL, fileName, ext);
			if (2048 - strlen(errorCode) - strlen(fileName) - 20 > 0) {
				strcat(errorCode, "\nFailed to convert: ");
				strcat(errorCode, fileName);
				strcat(errorCode, ".");
				strcat(errorCode, ext);
			}
		
		}
	}

	double timeInSec = (double)(clock() - start) / 1000;
	notifyUserAtEnd(timeInSec, numImg, numFailedFiles, errorCode);

	for (int i = 0; i < numImg; i++) {
		free(imgPaths[i]);
	}

	free(imgPaths);
	free(inputPath);
	free(outputDirPath); 

	return 0;
}


