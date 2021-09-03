#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

static const char* copyright_and_usage[] = {
  "Copyright 2013-2014, 2021 Vladimir Belousov (vlad.belos@gmail.com)",
  "https://github.com/VladimirBelousov/script-packer",
  "Licensed under the Apache License, Version 2.0 (the \"License\");",
  "you may not use this file except in compliance with the License.",
  "You may obtain a copy of the License at",
  "",
  "http://www.apache.org/licenses/LICENSE-2.0",
  "",
  "Unless required by applicable law or agreed to in writing, software",
  "distributed under the License is distributed on an \"AS IS\" BASIS,",
  "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.",
  "See the License for the specific language governing permissions and",
  "limitations under the License.",
  "",
  "The usage:",
  "",
  "srcpck [script_path&name] [interpreter_path&name] [optional_first_file2bind_path&name] ... [optional_Nth_file2bind_path&name] > [output_path&name].c",
  "",
  "A relative path ./ is needed when bound files and the interpreter are in the same folder with the output executable.",
  "",
  "Specify, please, at least the script file to pack and the file of a command interpreter.",
  0};

// The relative path to a current folder
static const char curdir[] = "./";

/* Hmalloc: malloc what is needed up to 4 GB max,
 * add 4 bytes (32 bit) padding and fill with zeros the allocation
 */
char* Hmalloc (unsigned int size)
{
  unsigned int s = (size + 4) & 0xfffffffc; /* 4 GB max */
  char* p = malloc (s);
  if (p != NULL)
    memset (p, 0, s);
  else {
    fprintf(stderr, "There is not enough memory for %d bytes\n", s);
    exit(1);
  }
  return (p);
}

// Two integers comparison functor for the standard sorting function qsort
int cmp(const void* a, const void* b) {
  return *(int*)a - *(int*)b;
}

/* Read from a file into a bytes array
 * path - a full path to read a file
 * dest - an address of a char array where to write
 * beg - an offset from the beginning of the file
 * size - a bytes quantity to read from the file
 */
void mapfile(char* path, char* dest, long beg, int size) {
  // The file handler to read a script's source
  HANDLE input_file = NULL;
  // How many bytes were read
  DWORD dwRead;
  // Open the file for reading only
  input_file = CreateFile(
    path,
    GENERIC_READ,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_READONLY,
    NULL);
  // Failed to open the file
  if (input_file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "The file %s was not opened\n", path);
    exit(1);
  }
  // Move the file reading pointer
  if (beg > 0)
    if (!SetFilePointer(input_file, beg, NULL, FILE_BEGIN)) {
      fprintf(stderr, "The file pointer was not moved to the %d byte\n", beg);
      CloseHandle(input_file);
      exit(1);
    }
  // Read the file
  if (!ReadFile(input_file, dest, size, &dwRead, NULL) || dwRead == 0) {
    fprintf(stderr, "Can't read the file %s\n", path);
    CloseHandle(input_file);
    exit(1);
  }
  // Close the file's handler
  CloseHandle(input_file);
}

int main(int argc, char **argv) {
  // The script's paths and the interpreter paths should be passed as a second and a third commandline arguments
  // The first commandline argument is the name of this executable itself
  if (argc < 3) {
    for (int i=0; copyright_and_usage[i]; i++) fprintf(stderr, "%s\n", copyright_and_usage[i]);
    exit(1);
  }

  // The structure which is needed for the stat function to get a file's size using the file's path
  struct stat st;
  // The index to the array of pointers to the commandline args,
  // the arg with the zero index is the name of the executable itself
  int optind = 1; // Init with the value of the first commandline arg
  // The reading beginning of the optional files to bind
  long zdvig = 0;
  // The reading length of the optional files to bind
  int dlina = 0;
  // The length of the file with script sources
  int scriptsize = 0;

  // Get the path of the executable file of the current process, i.e. get the absolute path for the relative one ./
  char szPath[0x250]; // 592 chars max
  DWORD dlszPath = GetModuleFileName(NULL, szPath, sizeof(szPath));
  // After the last backslash put the null terminator \0, that's how we remove the filename of a current process
  // in order to leave onle the path
  memset(strrchr((char*)szPath, 92 /* backslash ASCII dec code */) + 1, 0, 1);

  /// Reading the file with the script's content:
  // The pointer to the array of pointers on arrays of chars from files
  char** array = (char**)(malloc(sizeof(char*) * argc));
  // The array of byte lengths, which were read from the files
  // As the files are binary the strlen does not work - the null terminator \0 may be in any place of a binary file
  int* sizes = (int*)(malloc(sizeof(int) * argc));
  // Mesure the script's file size
  stat(argv[optind], &st);
  scriptsize = st.st_size;
  // The pointer to the pointer on the chars array, where the script's file will be read
  array[optind] = (char*)(malloc(sizeof(char) * scriptsize));
  // If there is not enough memory, i.e. array[optind] == NULL
  if (!array[optind]) {
    fprintf(stderr, "There is not enough memory for %d bytes\n", scriptsize);
    goto exitout; // Yep, guys, it is goto and it is in this century, are you surprised ? ;)
  }
  // Write the script's content, whose path is passed in argv[optind], into a chars array with address in array[optind]
  mapfile(argv[optind], array[optind], 0, scriptsize);
  ///

  static const char* sourcebegin[] = {
    #placeholder 1 content
    "static const char* files[] = {",
  0};

  // Output the beginning of the source code of the wrapper executable
  // The loop through all array elements with the new line added to the end of each string
  for (int i=0; sourcebegin[i]; i++) printf("%s\n", sourcebegin[i]);

  // Read other bound files, whose paths are passed as a commandline arguments,
  // a content of these files will be used in the decryption and encryption also,
  // so these files shouldn't be changed for a successful decrypting
  // The first must be the path to a command interpreter, other files' paths are optional
  for (optind=2; optind<argc; optind++) {
    // Output the paths from the array as a continuation to the static const char* files[] = {...
    printf("  \"%s\",\n", argv[optind]);
    // The array for an absolute path initially filled with zeros
    char* argabs = (char*)(Hmalloc(sizeof(char) * (strlen(argv[optind])+strlen(szPath))));
    // If a value in the argv[optind] begins with the relative path ./
    if (strncmp(argv[optind], (char*)curdir, 2) == 0) {
      // Change the relative path to an absolute path to the current directory
      strcat(argabs, (char*)szPath); // Put the abs path
      strncat(argabs, argv[optind]+2, strlen(argv[optind])-2); // Add the filename
    } else strcat(argabs, argv[optind]); // Copy without changes

    // Mesure the file size
    stat(argabs, &st);
    // Shift by 1680 bytes (the length of a standard header for an executable), if it is possible
    if (st.st_size >= 1680) zdvig = 1680;
    else zdvig = 0;
    // If it is possible then read bytes from the file by the length of the  script content
    if (scriptsize <= st.st_size - zdvig) dlina = scriptsize;
    else dlina = st.st_size - zdvig;
    // Allocate a byte array to read data from the file
    array[optind] = (char*)(malloc(sizeof(char) * dlina));
    // If there is not enough memory, i.e. array[optind] == NULL
    if (!array[optind]) {
      fprintf(stderr, "There is not enough memory for %s Kb\n", scriptsize);
      free(argabs);
      goto exitout;
    }
    // Read the file into the array[optind]
    mapfile(argabs, array[optind], zdvig, dlina);
    // Store the read length
    sizes[optind] = dlina;
    // Free previously allocated memory
    free(argabs);
  }

  static const char* sourcemiddle[] = {
    "  0};",
    #placeholder 2 content
  0};

  // Output the middle of the source code of the wrapper executable
  // The loop through all array elements with the new line added to the end of each string
  for (int i=0; sourcemiddle[i]; i++) printf("%s\n", sourcemiddle[i]);

  // Output the variable, which contains the quantity of elements in the array of files
  // Output the beginning of the data array
  printf("  const unsigned int klv = %d;\n  const unsigned int scriptsize = ", optind-2);

  /// The implementation of the key stretching algorithm through the segment dichotomy, the segment has length from 1 to 255
  /* length - the array of segments
   * sulength - the sorted array of unique segments
   * middle - the array of middles
   * i1, i2, i3 - different indexies
   * key_base_length, encrypted_length
   * mdl - the middle of a current segment
   * symb - the current byte of the script's content, which is crypting at this moment
   */
  unsigned int length[255] = {0}, sulength[255] = {0}, middle[255] = {0}, i1, i2, i3, key_base_length, encrypted_length, mdl;
  char symb;
  // Set a random number from the current CPU ticks as a start point for a pseudorandom, but actually determinated, sequence
  srand(GetTickCount());
  // Set the quantity of numbers for the base key from 30 to 60 bytes, i.e. from 240 to 480 bits
  unsigned int key_length = rand() % 31 + 30;
  // Output the bytes quantity in the script's content and the quantity of numbers in the base key
  // Cast the key_length from unsigned int 0..255 to char (which is signed by default) -128..127
  printf("%d;\n  const char data[] = {%d", scriptsize, key_length - 128);
  for (int i=0; i<key_length; i++) {
    // Fill the array of segments with a random numbers in the range from 2 to 254,
    // because the max segment is from 1 to 255
    length[i] = rand() % 253 + 2;
    // Output the values of the base key, casting from unsigned int 0..255 to char -128..127
    printf(",%d", length[i] - 128);
  }
  // Sort the array of initial segments
  qsort(length, key_length, sizeof(unsigned int), cmp);
  // Add the left boundary of the max segment to the array of sorted unique elements
  sulength[0] = 1;
  // Fill the array of sorted unique elements, i.e. sulength
  i1=1;for (int i=0; i<key_length-1; i++) if (length[i] != length[i+1]) { sulength[i1] = length[i]; i1++; }
  // Assign the last element of the source array to the array of sorted unique elements
  sulength[i1] = length[key_length-1];
  // Add the right boundary of the max segment to the array of sorted unique elements
  i1++;sulength[i1] = 255;
  // Key may be reduced due to repeated elements
  key_length = i1;
  // Stretch the key over the entire length of the script's content,
  // crypt each byte and output the result
  key_base_length = key_length;encrypted_length=0;
  do {
    // Create the sequence up to 254 bytes
    i1=0;do {
      // One walk through the array of segments
      i2=0;for (int i=0; i<key_length; i++) {
        // The integer middle of the segment
        mdl = (sulength[i] + sulength[i+1]) / 2;
        // If the integer middle of the segment exists
        if (mdl > sulength[i] && mdl < sulength[i+1]) {
          // Fill the array of middles
          middle[i1] = mdl; i1++;
          // Fill a new array of segments: the beginning of the segment and it's middle
          length[i2] = sulength[i]; i2++; length[i2] = mdl; i2++;
        } else {
          // If there is not a middle in the segment,
          // then fill a new array of segments with the beginning of the segment
          length[i2] = sulength[i]; i2++;
        }
      }
      // Assign the last element of the new array of segments
      length[i2] = sulength[key_length];
      // Assign the key_length to the index of the last element of the new array of segments
      key_length = i2;
      // Copy the new array of segments to the sulength array
      // The array of the segments was stretched by the middles of each segment
      memcpy(sulength, length, sizeof(unsigned int) * (i2+1));
    } while (key_length<254); // Until the array of the segments is less than 254 elements
    // The middle of the middles array
    mdl = i1 / 2;
    int last_elem = i1 - 1;
    // Copy the left half of the middles array
    memcpy(length, middle, sizeof(unsigned int) * mdl);
    // Copy the right half of the middles array
    memcpy(sulength, &middle[mdl], sizeof(unsigned int) * (i1 - mdl));
    // Now shuffle all cards thoroughly :)
    // Creating an new sequence of middles (each time an even number of unique elements):
    // the left half from the beginning and the right half from the end,
    // taking one element from the left half and one from the right and so on
    i1=0;for (int a=0, b=last_elem; a<mdl; a++, b--) {
      middle[i1] = length[a]; i1++; middle[i1] = sulength[b]; i1++;
    }
    /// Now let's crypt and output i1 bytes of the script's content
    for (int i=0; i<i1; i++) {
      // Go out if all content is encrypted already
      if (encrypted_length >= scriptsize) break;
      // One byte of the script's content XOR with a value from the middles array
      symb = *(array[1] + encrypted_length) ^ (char)middle[i];
      // Starting from the third commandline arg (the path to the interpreter)
      // mix a files content into the crypted result
      for (optind=2; optind<argc; optind++) {
        // The length of an array, which was read from the binary file
        i2 = sizes[optind];
        // The index, which indicates what byte to take from file,
        // which is calculated as a sum of the current encrypted length and a value from the middles array
        i3 = encrypted_length + middle[i];
        // If The index had reached the end of file then start from the beginning
        if (i3 >= i2) {
          i2 = i3 - i2 * (i3 / i2);
        } else {
          i2 = i3;
        }
        symb = symb ^ (*(array[optind]+i2));
      }
      // Output the crypted byte and shift to the next one
      printf(",%d", symb);encrypted_length++;
    }
    ///
    // The beginning of the max segment, which is equal to 1
    sulength[0] = 1;
    // Copy the key_base_length number of elements from the middles array as an initial array of segments for the next sequence
    memcpy(&sulength[1], middle, sizeof(unsigned int) * key_base_length);
    // The end of the max segment, which is equal to 255
    sulength[key_base_length+1] = 255;
    key_length = key_base_length + 2;
    // Sort the initial array of segments
    qsort(sulength, key_length, sizeof(unsigned int), cmp);
  // Until all script's content is not encrypted
  } while (encrypted_length < scriptsize);
  ///

  static const char * sourcend[] = {
    "};",
    #placeholder 3 content // The entire ./script.c file
    #placeholder 4 content
  0};

  // Output the end of the source code of the wrapper executable
  // The loop through all array elements with the new line added to the end of each string
  for (int i=0; sourcend[i]; i++) printf("%s\n", sourcend[i]);

exitout:
  // Free the allocated memory for the bytes arrays, which were read from files
  for (optind=1; optind<argc; optind++) free(array[optind]);
  // Free the allocated memory for the array of pointers
  free(array);
  // Free the allocated memory for the array of sizes of data read from files
  free(sizes);
}
