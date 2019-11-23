# gwatch
A program that watches a folder for file modifications and commits them to a git repository automatically

## How gwatch works
After gwatch is started it will watch a given folder and all of its subfolders (recursively) for changes. If a change occurs, a timer will be started (30s by default). After the timer expires, gwatch will create a new git commit with all the modifications. The timer is to prevent creating too many commits when there are a lot of modifications. In order for gwatch to successfully create commits, a git repository must be initialized in the watched folder.

Gwatch works on Linux and on Windows.

## Use cases
Gwatch can be used to track changes or backup files. E.g.:
- you are working on a document, an image file or a song and the software you are using does not support tracking of the changes. Gwatch will commit every time you save your work. This way you can go back to previous versions of your work using standard git commands.
- you are playing a game that keeps your progress in a save folder. If your save files become corrupted most of the time you will lose all your progress. Gwatch will protect you against such situation.

## Using gwatch
You will need to install git first if not already installed. Follow these steps to use gwatch:
- make sure there is a git repository initialized in the folder you want to watch; if not run `git init` from the command prompt or terminal.
- on Windows you can just copy and paste the `gwatch.exe` file to that folder and run it. On Linux type `./gwatch -r /path/to/watch`

That's it. Gwatch will start, watch the folder and create commits when necessary.

If you want you can also specify the timeout in seconds like this:

`./gwatch -r /path/to/watch -t 5` (on Linux)

`gwatch.exe -r C:\path\to\watch -t 5` (on Windows)

Lower values will result in a lot of created commits if changes happen often. If you omit the `-t` argument, gwatch will use the default 30s timeout.

## Important notes
- Make sure you are checked out on some branch. Gwatch will commit to that branch. If you are in detached HEAD state, gwatch will refuse to commit.

## Compiling on Windows
To compile on Windows you will need CMake 3.15 or newer and Visual Studio 2019. Earlier versions should work too but haven't been tested. Follow these steps:
- create a folder named `build` in the project's root
- open the command prompt in that folder and run:
- `cmake .. -A Win32`
- `cmake --build . --config Release`

By default the binary will be linked against static run-time library (`/MT` option for MSVC)

## Compiling on Linux
To compile on Linux you will need CMake 3.15 and a C compiler (e.g. GCC or Clang). Run:
- `mkdir build`
- `cd build`
- `cmake .. -DCMAKE_BUILD_TYPE=Release`
- `cmake --build .`
