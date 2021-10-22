/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <string.h>
#include <stdexcept>
#include "read_text.h"

string read_text(const char* path)
{
  FILE* fptr = fopen(path, "r");
  if (fptr == nullptr) {
    throw std::runtime_error(strerror(errno) + string(": ") + path);
  }

  fseek(fptr, 0, SEEK_END);
  size_t len = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);
  string content(len + 1, '\0');
  size_t size = fread(&content[0], 1, len, fptr);
  fclose(fptr);
  return content;
}