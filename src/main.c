/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/

int AcapRuntime(int argc, char* argv[]);

// Main entry for executable program
// to allow unittest of _main()
int main(int argc, char* argv[])
{
    return AcapRuntime(argc, argv);
}