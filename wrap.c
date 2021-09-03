/*! A program for the Windows platform, which launches a child process,
 *  passes a data stream on it's stdin and gets a data from it's output
 *  static const char* files[] - The array of paths to bound files
 */
#placeholder 1 start
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
#placeholder 1 end

#placeholder 2 start
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

/*! HANDLE - is a macro, which defines far pointers to descriptors and contexts of various objects
 */
HANDLE child_stdin_read = NULL;
HANDLE child_stdin_write = NULL;
HANDLE child_stdout_read = NULL;
HANDLE child_stdout_write = NULL;

/*! PROCESS_INFORMATION - is a macro, which is resolved into a far pointer to a structure,
 *  which is filled by the CreateProcess function with the info about a new spawned process and it's primary thread
 */
PROCESS_INFORMATION procinfo;

/*! Functions, which are defined after the main(), are declared here
 */
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

/*! The function, which reads a stdout of a child process and writes to this program's (a child's parent) stdout
 */
void ReadFromPipe(void) {
  DWORD dwRead, dwWritten, ExitCode;
  CHAR chBuf[BUFSIZE];
  BOOL bSuccess = FALSE;
  HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
  // Check the state of the child process
  bSuccess = GetExitCodeProcess(procinfo.hProcess, &ExitCode);
  // Loop while the child process is active
  while (ExitCode == STILL_ACTIVE) {
    if (!bSuccess) break;
    // Read the stdout of the child process as a regular file
    bSuccess = ReadFile(child_stdout_read, chBuf, BUFSIZE, &dwRead, NULL);
    /*! After the child process closes it's child_stdout_write descriptor the ReadFile returns an error
     *  ERROR_BROKEN_PIPE, and this loop becomes broken
     */
    if(!bSuccess || dwRead == 0) break;
    // Output to this program's stdout
    bSuccess = WriteFile(hParentStdOut, chBuf, dwRead, &dwWritten, NULL);
    if (!bSuccess || dwWritten == 0) break;
    // Check the state of the child process
    bSuccess = GetExitCodeProcess(procinfo.hProcess, &ExitCode);
    if (!bSuccess) break;
  }
  // Close this program's stdout
  CloseHandle(hParentStdOut);
}

int main(int argc, char *argv[]) {
  // Set a console's output to UTF-8 encoding
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
  /*! The SECURITY_ATTRIBUTES is a far pointer to a struct with security properties of a parent (this program)
   *  and sets whether these properties are inherited by child processes
   */
  SECURITY_ATTRIBUTES saAttr;
  // A size in bytes
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  // Let the child inherit properties of it's parent
  saAttr.bInheritHandle = TRUE;
  // Let the child has a security descriptor by default from the current process
  saAttr.lpSecurityDescriptor = NULL;
  /*! Let's create a pipe, where a child's stdout is bounbd to parent's handle
   *  CreatePipe == 0 on an error, CreatePipe == 1 on a success
   */
  if (!CreatePipe(&child_stdout_read, &child_stdout_write, &saAttr, 0))
    fprintf(stderr, "A StdoutRd CreatePipe error\n");
  /*! SetHandleInformation sets up properties for a stdout reading through the child_stdout_read
   *  HANDLE_FLAG_INHERIT - the child process, which was created with CreateProcess(bInheritHandles == TRUE),
   *  inherits a far pointer in the address space of it's parent
   *  SetHandleInformation == 0 on an error, SetHandleInformation == 1 on a success
   */
  if (!SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0))
    fprintf(stderr, "A Stdout SetHandleInformation error\n");
  // Similarly create a pipe, where stdin is bound to a parent's address space
  if (!CreatePipe(&child_stdin_read, &child_stdin_write, &saAttr, 0))
    fprintf(stderr, "A Stdin CreatePipe error\n");
  // Set up the properties of the stdin
  if (!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0))
    fprintf(stderr, "A Stdin SetHandleInformation error\n");

  // Get an absolute path to the current program and put it into the szPath
  char szPath[0x250]; // 592 chars max
  DWORD dlszPath = GetModuleFileName(NULL, szPath, sizeof(szPath));
  memset(strrchr((char*)szPath, 92 /* backslash ASCII dec code */) + 1, 0, 1);
  // The array for absolute paths initially filled with zeros
  char* argabs = (char*)(Hmalloc(sizeof(char) * (strlen(files[0]) + strlen(szPath))));
  // Change a relative path, which begins from ./, to an absolute path to the current program
  if (strncmp(files[0], (char*)curdir, 2) == 0) {
    // Replace the ./ with the absolute path
    strcat(argabs, (char *)szPath);
    strncat(argabs, files[0] + 2, strlen(files[0]) - 2);
  } else strcat(argabs, files[0]);
  // Create the child process with a bound to the already created stdin and stdout pipes
  CreateChildProcess(argabs);
  free(argabs);
  /*! Write to pipe, which is the stdin of the child process
   *  Data is written into a buffer of the pipe, so it is not needed to wait
   *  until the child process is launched to start writing
   */
  WriteToPipe(argc, argv);
  // Read from the pipe, which is the stdout of the child process
  ReadFromPipe();
  // Close the handles of the child process and it's main thread
  CloseHandle(procinfo.hProcess);
  CloseHandle(procinfo.hThread);
  // The remaining open handles are cleaned up when this process terminates.
  // To avoid resource leaks in a larger application, close handles explicitly.
  return 0;
}

// Create a child process with a binding to a previously created stdin abd stdout
void CreateChildProcess(char * shellpath) {
  /*! STARTUPINFO - a macro for a long pointer to a structure
   *  The STARTUPINFO structure is used with CreateProcess, CreateProcessAsUser, CreateProcessWithLogonW
   *  in order to define a console terminal, a desktop, a standard descriptor and a look of a main window of a new process
   */
  STARTUPINFO startinfo;
  BOOL bSuccess = FALSE;
  ZeroMemory( &procinfo, sizeof(PROCESS_INFORMATION) );
  ZeroMemory( &startinfo, sizeof(STARTUPINFO) );
  startinfo.cb = sizeof(STARTUPINFO);
  // Set stderror to the descriptor of the child stdout for writing
  startinfo.hStdError = child_stdout_write;
  // Set stdout to the descriptor of the child stdout for writing
  startinfo.hStdOutput = child_stdout_write;
  // Set stdin to the descriptor of the child stdout for reading
  startinfo.hStdInput = child_stdin_read;
  startinfo.dwFlags |= STARTF_USESTDHANDLES;
  // Create a child process
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
  // If an error occurs, exit the application.
  if (!bSuccess) fprintf(stderr, "A CreateProcess error\n");
  //else {
        // закрыть дескрипторы дочернего процесса и его основного потока из структуры PROCESS_INFORMATION
    // нужно не закрывать эти дескрипторы, для отслеживания статуса дочернего процесса
    //CloseHandle(procinfo.hProcess);
    //CloseHandle(procinfo.hThread);
  //}

  /*! Close the descriptors, which the child process is using (the pipe ends of the child process: child_stdin_read и child_stdout_write),
   *  in order to avoid a hanging of the child process together with a loop of a child process pipe's stdout reading
   *  in the ReadFromPipe function
   *  The child process on it's finish will close these descriptors from it's side and will be able to be finished by itself
   */
  if (!CloseHandle(child_stdin_read)) fprintf(stderr, "A StdInRd CloseHandle error\n");
  if (!CloseHandle(child_stdout_write)) fprintf(stderr, "A StdOutWr CloseHandle error\n");
}

// Write into the child process pipe stdin (actually write into a buffer)
void WriteToPipe(int argN, char *argM[]) {
  DWORD dwRead, dwWritten, i;
  BOOL bSuccess = FALSE;
#placeholder 2 end
  /*! The text below until the "#placeholder 4 start" won't be included in to the frame.c source code,
   *  after the building a code from the script.c will be included instead
   */
  int * data [] = {
    // The shell will get the command: echo \"Hello, world!\"
    "echo \\\"Hello, world!\\\"\n",
    "whoami\n",
    "\necho \"<<EOF>>\"\n",
  0};
  // Loop over all array items
  for (i=0; data[i]; i++) {
    // Write into the child_stdin_write as in a file
    bSuccess = WriteFile(child_stdin_write, data[i], strlen(data[i]), &dwWritten, NULL);
    if (!bSuccess || dwWritten == 0) break;
  }
#placeholder 4 start
  // The child_stdin pipe will be closed, because the last it's descriptor will closed below
  if (!CloseHandle(child_stdin_write)) fprintf(stderr, "A StdInWr CloseHandle error\n");
}
#placeholder 4 end
