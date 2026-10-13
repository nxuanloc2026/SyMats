Symats for Windows x64

Extract the entire ZIP, then double-click run-symats.bat.
Enter an expression such as 2+3 or D[x^3,x]. Type quit to exit.
To run a script from a terminal: symats-cli.exe "path\to\file.sym"

This is the native command-line release. No Node.js or separate Visual C++
runtime installation is required. Optional Giac/SUNDIALS backends and the
xeus Jupyter kernel are not included in this package.

The notebook UI is a development preview in the source repository. It still
requires a desktop/server host connecting the browser to the C++ bridge;
opening its HTML directly cannot start an interactive notebook kernel.

Symats is licensed under GPL-3.0-or-later. See LICENSE and SOURCE.txt.
