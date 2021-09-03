  /*! The part of the WriteToPipe() body from the wrap.c
   *  static const char* files[] - The array of paths to bound files
   *  const unsigned int klv - The quantity of elements in the array of files
   *  const unsigned int scriptsize - The size of the script's body in bytes
   *  const char data[] - It is the array, which has a number of chars in a base key, a base key with repeated symbols
   *  and an encrypted script body
   */
  // The structure which is needed for the stat function to get a file's size using the file's path
  struct stat st;
  // The reading beginning of the optional files to bind
  long zdvig = 0;
  // The reading length of the optional files to bind
  int dlina = 0;

  // Get the path of the executable file of the current process, i.e. get the absolute path for the relative one ./
  char szPath[0x250]; // 592 chars max
  DWORD dlszPath = GetModuleFileName(NULL, szPath, sizeof(szPath));
  // After the last backslash put the null terminator \0, that's how we remove the filename of a current process
  // in order to leave onle the path
  memset(strrchr((char*)szPath, 92 /* backslash ASCII dec code */) + 1, 0, 1);

  // The pointer to the array of pointers on arrays of chars from files
  char** array = (char**)(malloc(sizeof(char*) * klv));
  // The array of byte lengths, which were read from the files
  // As the files are binary the strlen does not work - the null terminator \0 may be in any place of a binary file
  int* sizes = (int*)(malloc(sizeof(int) * klv));

  // Read other bound files, whose paths are passed in the array static const char* files[] = {...},
  // a content of these files will be used in the decryption and encryption also,
  // so these files shouldn't be changed for a successful decrypting
  // The first must be the path to a command interpreter, other files' paths are optional
  for (int optind=0; optind<klv; optind++) {
    // The array for an absolute path initially filled with zeros
    char* argabs = (char*)(Hmalloc(sizeof(char) * (strlen(files[optind]) + strlen(szPath))));
    // If a value in the files[optind] begins with the relative path ./
    if (strncmp(files[optind], (char*)curdir, 2) == 0) {
      // Change the relative path to an absolute path to the current directory
      strcat(argabs, (char *)szPath); // Put the abs path
      strncat(argabs, files[optind]+2, strlen(files[optind])-2); // Add the filename
    } else strcat(argabs, files[optind]); // Copy without changes
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
  // Pass to the script the arguments, which were passed to this executable argv[1..argc]
  // without the name of the script itself, because the 'set --' does not change the $0
  char cmd[] = "set -- ";
  bSuccess = WriteFile(child_stdin_write, &cmd, sizeof(cmd), &dwWritten, NULL);
  char quote[] = "\"";
  char br[] = " ";
  for (int i=1; i<argN; i++) {
    // Write double quotes
    bSuccess = WriteFile(child_stdin_write, &quote, sizeof(quote), &dwWritten, NULL);
    // Write an argument
    bSuccess = WriteFile(child_stdin_write, &(*argM[i]), strlen(&(*argM[i])), &dwWritten, NULL);
    // Write double quotes
    bSuccess = WriteFile(child_stdin_write, &quote, sizeof(quote), &dwWritten, NULL);
    // Write a space
    bSuccess = WriteFile(child_stdin_write, &br, sizeof(br), &dwWritten, NULL);
  }
  // Write a new line
  char nline [] = "\n";
  bSuccess = WriteFile(child_stdin_write, &nline, sizeof(nline), &dwWritten, NULL);

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
  // Cast char (it is signed by default) -128..127 to unsigned int 0..255
  unsigned int init_key_length = data[0] + 128;
  // Fill an array with initial segments values from the data[] array,
  // casting from signed char to unsigned int
  for (int i=0; i<init_key_length; i++) length[i] = data[1+i] + 128;
  // Sort the array of initial segments
  qsort(length, init_key_length, sizeof(unsigned int), cmp);
  // Add the left boundary of the max segment to the array of sorted unique elements
  sulength[0] = 1;
  // Fill the array of sorted unique elements, i.e. sulength
  i1=1;for (int i=0; i<init_key_length-1; i++) if (length[i] != length[i+1]) { sulength[i1] = length[i]; i1++; }
  // Assign the last element of the source array to the array of sorted unique elements
  sulength[i1] = length[init_key_length-1];
  // Add the right boundary of the max segment to the array of sorted unique elements
  i1++;sulength[i1] = 255;
  // Key may be reduced due to repeated elements
  unsigned int key_length = i1;
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
      symb = data[1+init_key_length+encrypted_length] ^ (char)middle[i];
      // Starting from the third commandline arg (the path to the interpreter)
      // mix a files content into the crypted result
      for (int optind=0; optind<klv; optind++) {
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
      // Write into the pipe child_stdin_write a one decrypted byte of the script code
      bSuccess = WriteFile(child_stdin_write, &symb, sizeof(char) /* write one byte */, &dwWritten, NULL);encrypted_length++;
      if (!bSuccess || dwWritten == 0) break;
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
exitout:
  // Free the allocated memory for the bytes arrays, which were read from files
  for (int optind=0; optind<klv; optind++) free(array[optind]);
  // Free the allocated memory for the array of pointers
  free(array);
  // Free the allocated memory for the array of sizes of data read from files
  free(sizes);
