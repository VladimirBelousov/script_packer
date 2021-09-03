# The Script Packer - srcpck

## The script packer which uses a key stretching algorithm through the segment dichotomy to encrypt and decrypt an embedded script body

### The usage

- Embed a script into a C file:
  - `srcpck [script_path&name] [interpreter_path&name] [optional_first_file2bind_path&name] ... [optional_Nth_file2bind_path&name] > [output_path&name].c`
- Compile into an executable file:
  - `gcc [output_path&name].c`
- A relative path `./` is needed when bound files and the interpreter are in the same folder with the output executable

### Dependencies

- Currently the srcpck works under Windows platform only, because it uses WinAPI in part of file operations and threading, with the exception of these it depends on C runtime only
- Currently commandline arguments are passed to the embedded script as a parameters of the "set --" command, therefore that works only for bash scripts only
- To build from sources one need the bash, sed and gcc utilities

### To build from sources just run the build command: ./build.sh

### The principles of work

- A script is encrypted and embed into an executable file
- When the output executable file is invoked the script body is decrypting byte per byte and is transmitted through a stdin to the bound interpreter, the script's output is sent to an output executable file's stdout in the UTF-8 encoding
- The script has all commandline arguments, which were passed to it's executable container
- The embed script is not able to read it's stdin and would hang on that operation, because the script is passed to the interpreter as a byte stream, not as a file, in order to hide thoroughly the script's source code
- The bound interpreter content, as well as the bound files content (optional), participate in the encryption and decryption process, therefore if even one byte of the bound data is changed then the embed script would be broken. So include the interpreter in to the pack with the output executable and use the relative `./` path to build with it. It is very crucial for the script's sources protection: without the binding it would be possible to rename a `cat` command to the interpreter name and see all the script's sources freely
- The bound files (optional) and the bound interpreter content participates in the encryption and decryption process with the 1680 bytes shift from the beginning (by the length of a standard header for executable)
- The encryption and the decryption is done using XOR binary operation with the script's content, the stretched key and the content of the bound interpreter and other optional bound files in non-linear sequence

### The key stretching algorithm through the segment dichotomy

- The key is stretched to the length of an object for the cryptological operations
- The key is a set of unrepeated one byte numbers from 2 to 254 followed by each other
- Each combination is derived from it's own set of segments within the main segment from 1 to 255 through the method of segments dichotomy, so the distribution is pretty similar to uniform
- The start combination of the key sequence is random and is unique for each packing

### TODO

- Implement POSIX file opertions and threading in order to use the srcpck on UNIX platforms (Linux, Android and Mac OS X)
- Implement a passing of commandline arguments to embedded scripts for other interpreters, not only the bash
