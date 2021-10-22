typedef unsigned char uchar;

void ReadImage(const char *fileName, uchar **pixels, int *width, int *height, int *bytesPerPixel);
void WriteImage(const char *fileName, uchar *pixels, int width, int height, int bytesPerPixel);