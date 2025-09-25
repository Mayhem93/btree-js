# B+Tree.JS

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-brightgreen.svg)](https://opensource.org/licenses/MPL-2.0) [![License](https://img.shields.io/badge/License-Boost_1.0-lightblue.svg)](https://www.boost.org/LICENSE_1_0.txt)

## ⚠️Disclaimer

This project was done for my own pleasure and education as I have not coded in a language like C++ since college. This was made with the help of AI (conversational Copilot).

## Prerequisites

### vcpkg

* Install vcpkg put in $PATH and make sure to add a user env variable "VCPKG_ROOT" to where you installed it
* Do a one time `vcpkg integrate install`. This will integrate the tool with msvs build tools
* In the root folder of the project do `vcpkg install`

### Nodejs part

* `node-gyp` for nodejs20

### MSVS

* The builds I made targeted `/std:c++latest`
* I had MS VS 2022 (community edition works fine)
* Make sure to run any command (or start vscode) using the "Developer Powershell for VS 20XX" (cmd works as well, but no one should use it in 2025)

## Building steps

At the moment I've only built this on Windows-x64 using `node-gyp` and vscode for the test code

### VSCode Task for building the test.cpp code

```json
{
    "type": "shell",
    "label": "Build test.cpp",
    "command": "cl.exe",
    "args": [
        "/O2",
        "/DNDEBUG",
        "/DBTREE_ENABLE_JSON",
        "/Zi",
        "/EHsc",
        "/std:c++latest",
        "/nologo",
        "/I${workspaceFolder}\\src",
        "/I${workspaceFolder}\\vcpkg_installed\\x64-windows\\include",
        "/I${env:USERPROFILE}\\AppData\\Local\\node-gyp\\Cache\\20.7.0\\include\\node",
        "/Fe${workspaceFolder}\\src\\test.exe",
        "${workspaceFolder}\\src\\test.cpp"
    ],
    "options": {
        "cwd": "${fileDirname}"
    },
    "problemMatcher": [
        "$msCompile"
    ],
    "group": "build",
    "detail": "Testing btree"
}
```

### VSCode run/debug configuration

```json
{
    "name": "Launch test.exe",
    "type": "cppvsdbg",
    "request": "launch",
    "program": "${workspaceFolder}/src/test.exe",
    "args": [],
    "cwd": "${workspaceFolder}/src",
    "console": "integratedTerminal",
    "stopAtEntry": false
}
```
