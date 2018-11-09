#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "bmp.h"

//Validation functions
int ValidateFile(FILE* file);
int ValidateFormat(BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader);

//Resizing
void Resize(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader, double scaler);
void ScaleUp(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader, BITMAPFILEHEADER originalFileHeader, BITMAPINFOHEADER originalInfoHeader, double scaler);
void ScaleDown(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader, BITMAPFILEHEADER originalFileHeader, BITMAPINFOHEADER originalInfoHeader, double scaler);
void Copy(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader);

int main(int argc, char *argv[])
{
    //Check if the user entered 3 arguments
    if (argc != 4)
    {
        fprintf(stderr, "Usage: ./resize f infile outfile\n"); //Show the user how to use the program if used wrong
        return 1;
    }

    double scaler = atof(argv[1]);
    char* readFileName = argv[2];
    char* writeFileName = argv[3];

    FILE* readFile = fopen(readFileName, "r");
    FILE* writeFile = fopen(writeFileName, "w");

    //Check if the files are valid
    if (!ValidateFile(readFile) || !ValidateFile(writeFile))
    {
        fprintf(stderr, "One of the given files(names) can not be used\n"); //If not valid exit the program
        fclose(readFile);
        fclose(writeFile);
        return 2;
    }


    //The BMP file/info headers needed for processing
    BITMAPFILEHEADER bmpFileHeader;
    fread(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, readFile);

    BITMAPINFOHEADER bmpInfoHeader;
    fread(&bmpInfoHeader, sizeof(BITMAPINFOHEADER), 1, readFile);

    //Check if the original file format is supported
    if (!ValidateFormat(bmpFileHeader, bmpInfoHeader))
    {
        fprintf(stderr, "Unsupported file format\n"); //If not supported exit the program
        fclose(readFile);
        fclose(writeFile);
        return 4;
    }

    //Start the resizing
    Resize(readFile, writeFile, bmpFileHeader, bmpInfoHeader, scaler);

    //Close all the open files
    fclose(readFile);
    fclose(writeFile);

    return 0;
}

//Validation functions that validates the given file
//Returns false is the file is not valid
int ValidateFile(FILE* file)
{
    if (file == NULL)
    {
        return 0;
    }

    return 1;
}

//Validate if the file has the supported format
int ValidateFormat(BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader)
{
    // ensure infile is (likely) a 24-bit uncompressed BMP 4.0
    if (fileHeader.bfType != 0x4d42 || fileHeader.bfOffBits != 54 || infoHeader.biSize != 40 || infoHeader.biBitCount != 24 || infoHeader.biCompression != 0)
    {
        return 0; //return false
    }

    return 1;
}

//The main resize function
void Resize(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader, double scaler)
{
    BITMAPFILEHEADER originalFileHeader = fileHeader;
    BITMAPINFOHEADER originalInfoHeader = infoHeader;

    infoHeader.biWidth *= scaler;
    infoHeader.biHeight *= scaler;

    // determine padding for new file
    int padding = (4 - (infoHeader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    infoHeader.biSizeImage = ((sizeof(RGBTRIPLE) * infoHeader.biWidth) + padding) * abs(infoHeader.biHeight);
    fileHeader.bfSize = infoHeader.biSizeImage + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    if (scaler == 1) //If the user wants to keep the same size some how
    {
        Copy(readFile, writeFile, originalFileHeader, originalInfoHeader);
    }
    else if (scaler > 1) //If the user wants to make the bmp bigger
    {
        ScaleUp(readFile, writeFile, fileHeader, infoHeader, originalFileHeader, originalInfoHeader, scaler);
    }
    else if (scaler < 1) //If the user wants to make the bmp smaller
    {
        ScaleDown(readFile, writeFile, fileHeader, infoHeader, originalFileHeader, originalInfoHeader, scaler);
    }

}

//Scales the bmp up
void ScaleUp(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader, BITMAPFILEHEADER originalFileHeader, BITMAPINFOHEADER originalInfoHeader, double scaler)
{

    //Calculate the padding in the scaled file and the original file
    int originalPadding = (4 - (originalInfoHeader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;
    int padding = (4 - (infoHeader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    //Write the header files to the new bmp
    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, writeFile);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, writeFile);

    //Loop through original pixels vertical
    for (int i = 0, pixelHeight = abs(originalInfoHeader.biHeight); i < pixelHeight; i++)
    {
        //RGBTRIPLE array so we can store the scaled row in pixels
        //We use this later to duplicate the horizontal row vertically
        RGBTRIPLE rgbTripleRow[infoHeader.biWidth];
        int index = 0;

        //loop through original row
        for (int j = 0; j < originalInfoHeader.biWidth; j++)
        {
            //The current original pixel
            RGBTRIPLE rgbTripleCurrent;

            //Read the current pixel
            fread(&rgbTripleCurrent, sizeof(RGBTRIPLE), 1, readFile);

            //Duplicate the pixel x amount of times horizontally
            for (int k = 0; k < scaler; k++)
            {
                //Save the current pixel so we can reduplicate next row if needed
                rgbTripleRow[index] = rgbTripleCurrent;
                fwrite(&rgbTripleCurrent, sizeof(RGBTRIPLE), 1, writeFile);
                index++;
            }
        }

        //Add padding to scaled version / finish scaled row
        for (int j = 0; j < padding; j++)
        {
            fputc(0x00, writeFile);
        }

        //Duplicate the horizontal row vertically
        for (int j = 0; j < scaler - 1; j++)
        {
            //Write all the pixels stored in the RGBTRIPLE array
            for (int k = 0; k < infoHeader.biWidth; k++)
            {
                fwrite(&rgbTripleRow[k], sizeof(RGBTRIPLE), 1, writeFile);
            }
            //Added needed padding to duplicated row
            for (int k = 0; k < padding; k++)
            {
                fputc(0x00, writeFile);
            }
        }

        //Let the cursor skip the padding in the original file so we can continue on the next line
        fseek(readFile, originalPadding, SEEK_CUR);
    }
}

void ScaleDown(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader, BITMAPFILEHEADER originalFileHeader, BITMAPINFOHEADER originalInfoHeader, double scaler)
{
    //Get padding
    int originalPadding = (4 - (originalInfoHeader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;
    int padding = (4 - (infoHeader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    //Calculate which pixels we are going to skip
    int heightPixelToSkip = round(abs(originalInfoHeader.biHeight) / (abs(originalInfoHeader.biHeight) - abs(infoHeader.biHeight)));
    int widthPixelToSkip = round(originalInfoHeader.biWidth / (originalInfoHeader.biWidth - infoHeader.biWidth));

    //Write the header files to the new bmp
    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, writeFile);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, writeFile);

    //Start counting vertically
    int heightCounter = 1;

    for (int i = 0, pixelHeight = abs(originalInfoHeader.biHeight); i < pixelHeight; i++)
    {
        if (heightCounter == heightPixelToSkip + 1)
        {
            heightCounter = 1;
        }
        int widthCounter = 1;

        for (int j = 0; j < originalInfoHeader.biWidth; j++)
        {
            if (widthCounter == widthPixelToSkip + 1)
            {
                widthCounter = 1;
            }

            //Get current pixel
            RGBTRIPLE rgbTripleCurrent;
            fread(&rgbTripleCurrent, sizeof(RGBTRIPLE), 1, readFile);

            //Check if we are going to write this pixel to the new bmp
            if (widthCounter != widthPixelToSkip && heightCounter != heightPixelToSkip)
            {
                fwrite(&rgbTripleCurrent, sizeof(RGBTRIPLE), 1, writeFile);
            }

            widthCounter++;
        }

        if (heightCounter != heightPixelToSkip) //If we are not copying this line don't put any padding in the file
        {
            for (int j = 0; j < padding; j++)
            {
                fputc(0x00, writeFile);
            }
        }

        //Move the cursor over the padding in the read file (original file)
        fseek(readFile, originalPadding, SEEK_CUR);

        heightCounter++;
    }
}

void Copy(FILE* readFile, FILE* writeFile, BITMAPFILEHEADER fileHeader, BITMAPINFOHEADER infoHeader)
{
    //Get padding
    int padding = (4 - (infoHeader.biWidth * sizeof(RGBTRIPLE)) % 4) % 4;

    //Write the header files to the new bmp
    fwrite(&fileHeader, sizeof(BITMAPFILEHEADER), 1, writeFile);
    fwrite(&infoHeader, sizeof(BITMAPINFOHEADER), 1, writeFile);

    for (int i = 0, pixelHeight = abs(infoHeader.biHeight); i < pixelHeight; i++)
    {
        for (int j = 0; j < infoHeader.biWidth; j++)
        {
            //Get current pixel
            RGBTRIPLE rgbTripleCurrent;
            fread(&rgbTripleCurrent, sizeof(RGBTRIPLE), 1, readFile);

            //Write the pixel to a new file
            fwrite(&rgbTripleCurrent, sizeof(RGBTRIPLE), 1, writeFile);
        }

        //Write any needed padding in the file
        for (int j = 0; j < padding; j++)
        {
            fputc(0x00, writeFile);
        }

        //Move the cursor over the padding in the read file (original file)
        fseek(readFile, padding, SEEK_CUR);
    }

}