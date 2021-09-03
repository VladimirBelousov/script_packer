#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#define BUFSIZE 4096
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
  " ",
  "A relative path ./ is needed when bound files and the interpreter are in the same folder with the output executable.",
  "",
  "Specify, please, at least the script file to pack and the file of a command interpreter.",
  0};
static const char curdir[] = "./";
static const char* files[] = {
  "./bash.exe",
  0};
char* Hmalloc (unsigned int size)
{
  unsigned int s = size + 1;
  char* p = malloc (s);
  if (p != NULL)
    memset (p, 0, s);
  else {
    fprintf(stderr, "There is not enough memory for %d bytes\n", s);
    exit(1);
  }
  return (p);
}
HANDLE child_stdin_read = NULL;
HANDLE child_stdin_write = NULL;
HANDLE child_stdout_read = NULL;
HANDLE child_stdout_write = NULL;
PROCESS_INFORMATION procinfo;
void CreateChildProcess(char *shellpath);
void WriteToPipe(int argN, char *argM[]);
int cmp(const void* a, const void* b) {
  return *(int*)a - *(int*)b;
}
void mapfile(char* path, char* dest, long beg, int size) {
  HANDLE input_file = NULL;
  DWORD dwRead;
  input_file = CreateFile(
    path,
    GENERIC_READ,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_READONLY,
    NULL);
  if (input_file == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "The file %s was not opened\n", path);
    exit(1);
  }
  if (beg > 0)
    if (!SetFilePointer(input_file, beg, NULL, FILE_BEGIN)) {
      fprintf(stderr, "The file pointer was not moved to the %d byte\n", beg);
      CloseHandle(input_file);
      exit(1);
    }
  if (!ReadFile(input_file, dest, size, &dwRead, NULL) || dwRead == 0) {
    fprintf(stderr, "Can't read the file %s\n", path);
    CloseHandle(input_file);
    exit(1);
  }
  CloseHandle(input_file);
}
void ReadFromPipe(void) {
  DWORD dwRead, dwWritten, ExitCode;
  CHAR chBuf[BUFSIZE];
  BOOL bSuccess = FALSE;
  HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
  bSuccess = GetExitCodeProcess(procinfo.hProcess, &ExitCode);
  while (ExitCode == STILL_ACTIVE) {
    if (!bSuccess) break;
    bSuccess = ReadFile(child_stdout_read, chBuf, BUFSIZE, &dwRead, NULL);
    if(!bSuccess || dwRead == 0) break;
    bSuccess = WriteFile(hParentStdOut, chBuf, dwRead, &dwWritten, NULL);
    if (!bSuccess || dwWritten == 0) break;
    bSuccess = GetExitCodeProcess(procinfo.hProcess, &ExitCode);
    if (!bSuccess) break;
  }
  CloseHandle(hParentStdOut);
}
int main(int argc, char *argv[]) {
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
  SECURITY_ATTRIBUTES saAttr;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;
  if (!CreatePipe(&child_stdout_read, &child_stdout_write, &saAttr, 0))
    fprintf(stderr, "A StdoutRd CreatePipe error\n");
  if (!SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0))
    fprintf(stderr, "A Stdout SetHandleInformation error\n");
  if (!CreatePipe(&child_stdin_read, &child_stdin_write, &saAttr, 0))
    fprintf(stderr, "A Stdin CreatePipe error\n");
  if (!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0))
    fprintf(stderr, "A Stdin SetHandleInformation error\n");
  char szPath[0x250]; // 592 chars max
  DWORD dlszPath = GetModuleFileName(NULL, szPath, sizeof(szPath));
  memset(strrchr((char*)szPath, 92 /* backslash ASCII dec code */) + 1, 0, 1);
  char* argabs = (char*)(Hmalloc(sizeof(char) * (strlen(files[0]) + strlen(szPath))));
  if (strncmp(files[0], (char*)curdir, 2) == 0) {
    strcat(argabs, (char *)szPath);
    strncat(argabs, files[0] + 2, strlen(files[0]) - 2);
  } else strcat(argabs, files[0]);
  CreateChildProcess(argabs);
  free(argabs);
  WriteToPipe(argc, argv);
  ReadFromPipe();
  CloseHandle(procinfo.hProcess);
  CloseHandle(procinfo.hThread);
  return 0;
}
void CreateChildProcess(char * shellpath) {
  STARTUPINFO startinfo;
  BOOL bSuccess = FALSE;
  ZeroMemory( &procinfo, sizeof(PROCESS_INFORMATION) );
  ZeroMemory( &startinfo, sizeof(STARTUPINFO) );
  startinfo.cb = sizeof(STARTUPINFO);
  startinfo.hStdError = child_stdout_write;
  startinfo.hStdOutput = child_stdout_write;
  startinfo.hStdInput = child_stdin_read;
  startinfo.dwFlags |= STARTF_USESTDHANDLES;
  bSuccess = CreateProcess(shellpath,  /// A command to execute; if NULL then a first word in a command line
    NULL,     /// A command line
    NULL,          /// Process security attributes
    NULL,          /// Primary thread security attributes
    TRUE,          /// Descriptors are inhereted
    0,             /// Creation flags
    NULL,          /// Use parent's environment
    NULL,          /// Use parent's current directory
    &startinfo,  /// The address to read the STARTUPINFO struct before the process creation
    &procinfo);  /// The address to fill the PROCESS_INFORMATION struct after the process creation
  if (!bSuccess) fprintf(stderr, "A CreateProcess error\n");
  if (!CloseHandle(child_stdin_read)) fprintf(stderr, "A StdInRd CloseHandle error\n");
  if (!CloseHandle(child_stdout_write)) fprintf(stderr, "A StdOutWr CloseHandle error\n");
}
void WriteToPipe(int argN, char *argM[]) {
  DWORD dwRead, dwWritten, i;
  BOOL bSuccess = FALSE;
  const unsigned int klv = 1;
  const unsigned int scriptsize = 155;
  const char data[] = {-89,121,124,115,-23,-28,38,-124,-67,-14,-87,95,-54,37,97,-97,89,-101,-119,-16,126,-58,81,115,-55,27,-109,32,76,100,123,-104,-40,78,-76,-122,96,49,-114,-1,126,-10,119,-4,-94,54,69,-69,-103,-101,112,70,-102,-124,-76,-12,-7,7,14,68,-83,18,112,-25,82,-19,59,-29,-80,40,9,9,-34,-35,-72,42,69,116,3,-11,-7,106,-46,108,-18,72,68,109,-125,76,84,-36,-15,-40,-15,-107,-112,87,92,-54,-109,126,-47,-123,-126,-79,-51,-45,-111,-108,54,25,-11,-76,-31,-77,89,64,86,-100,-74,-54,-1,42,-85,72,-95,60,27,-52,-86,-118,-21,-57,59,68,85,46,11,-49,-34,8,33,94,10,-114,46,67,-78,67,-105,108,-34,26,79,43,87,-76,-86,-17,-126,-73,-99,58,91,-76,-86,40,-84,46,-73,-74,80,-16,73,29,-126,72,-60,8,-36,-23,82,-71,-62,-90,113,111,-68,76,-67,-32,-100,32,-20};
  struct stat st;
  long zdvig = 0;
  int dlina = 0;
  char szPath[0x250]; // 592 chars max
  DWORD dlszPath = GetModuleFileName(NULL, szPath, sizeof(szPath));
  memset(strrchr((char*)szPath, 92 /* backslash ASCII dec code */) + 1, 0, 1);
  char** array = (char**)(malloc(sizeof(char*) * klv));
  int* sizes = (int*)(malloc(sizeof(int) * klv));
  for (int optind=0; optind<klv; optind++) {
    char* argabs = (char*)(Hmalloc(sizeof(char) * (strlen(files[optind]) + strlen(szPath))));
    if (strncmp(files[optind], (char*)curdir, 2) == 0) {
      strcat(argabs, (char *)szPath); // Put the abs path
      strncat(argabs, files[optind]+2, strlen(files[optind])-2); // Add the filename
    } else strcat(argabs, files[optind]); // Copy without changes
    stat(argabs, &st);
    if (st.st_size >= 1680) zdvig = 1680;
    else zdvig = 0;
    if (scriptsize <= st.st_size - zdvig) dlina = scriptsize;
    else dlina = st.st_size - zdvig;
    array[optind] = (char*)(malloc(sizeof(char) * dlina));
    if (!array[optind]) {
      fprintf(stderr, "There is not enough memory for %s Kb\n", scriptsize);
      free(argabs);
      goto exitout;
    }
    mapfile(argabs, array[optind], zdvig, dlina);
    sizes[optind] = dlina;
    free(argabs);
  }
  char cmd[] = "set -- ";
  bSuccess = WriteFile(child_stdin_write, &cmd, sizeof(cmd), &dwWritten, NULL);
  char quote[] = "\"";
  char br[] = " ";
  for (int i=1; i<argN; i++) {
    bSuccess = WriteFile(child_stdin_write, &quote, sizeof(quote), &dwWritten, NULL);
    bSuccess = WriteFile(child_stdin_write, &(*argM[i]), strlen(&(*argM[i])), &dwWritten, NULL);
    bSuccess = WriteFile(child_stdin_write, &quote, sizeof(quote), &dwWritten, NULL);
    bSuccess = WriteFile(child_stdin_write, &br, sizeof(br), &dwWritten, NULL);
  }
  char nline [] = "\n";
  bSuccess = WriteFile(child_stdin_write, &nline, sizeof(nline), &dwWritten, NULL);
  unsigned int length[255] = {0}, sulength[255] = {0}, middle[255] = {0}, i1, i2, i3, key_base_length, encrypted_length, mdl;
  char symb;
  unsigned int init_key_length = data[0] + 128;
  for (int i=0; i<init_key_length; i++) length[i] = data[1+i] + 128;
  qsort(length, init_key_length, sizeof(unsigned int), cmp);
  sulength[0] = 1;
  i1=1;for (int i=0; i<init_key_length-1; i++) if (length[i] != length[i+1]) { sulength[i1] = length[i]; i1++; }
  sulength[i1] = length[init_key_length-1];
  i1++;sulength[i1] = 255;
  unsigned int key_length = i1;
  key_base_length = key_length;encrypted_length=0;
  do {
    i1=0;do {
      i2=0;for (int i=0; i<key_length; i++) {
        mdl = (sulength[i] + sulength[i+1]) / 2;
        if (mdl > sulength[i] && mdl < sulength[i+1]) {
          middle[i1] = mdl; i1++;
          length[i2] = sulength[i]; i2++; length[i2] = mdl; i2++;
        } else {
          length[i2] = sulength[i]; i2++;
        }
      }
      length[i2] = sulength[key_length];
      key_length = i2;
      memcpy(sulength, length, sizeof(unsigned int) * (i2+1));
    } while (key_length<254); // Until the array of the segments is less than 254 elements
    mdl = i1 / 2;
    int last_elem = i1 - 1;
    memcpy(length, middle, sizeof(unsigned int) * mdl);
    memcpy(sulength, &middle[mdl], sizeof(unsigned int) * (i1 - mdl));
    i1=0;for (int a=0, b=last_elem; a<mdl; a++, b--) {
      middle[i1] = length[a]; i1++; middle[i1] = sulength[b]; i1++;
    }
    for (int i=0; i<i1; i++) {
      if (encrypted_length >= scriptsize) break;
      symb = data[1+init_key_length+encrypted_length] ^ (char)middle[i];
      for (int optind=0; optind<klv; optind++) {
        i2 = sizes[optind];
        i3 = encrypted_length + middle[i];
        if (i3 >= i2) {
          i2 = i3 - i2 * (i3 / i2);
        } else {
          i2 = i3;
        }
        symb = symb ^ (*(array[optind]+i2));
      }
      bSuccess = WriteFile(child_stdin_write, &symb, sizeof(char) /* write one byte */, &dwWritten, NULL);encrypted_length++;
      if (!bSuccess || dwWritten == 0) break;
    }
    sulength[0] = 1;
    memcpy(&sulength[1], middle, sizeof(unsigned int) * key_base_length);
    sulength[key_base_length+1] = 255;
    key_length = key_base_length + 2;
    qsort(sulength, key_length, sizeof(unsigned int), cmp);
  } while (encrypted_length < scriptsize);
exitout:
  for (int optind=0; optind<klv; optind++) free(array[optind]);
  free(array);
  free(sizes);
  if (!CloseHandle(child_stdin_write)) fprintf(stderr, "A StdInWr CloseHandle error\n");
}
