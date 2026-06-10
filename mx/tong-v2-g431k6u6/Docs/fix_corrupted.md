1. Close STM32CubeIDE completely.
1. Open your project folder in Windows File Explorer / Linux Finder.
1. Delete the `Debug` and `Release` folders entirely (these contains the broken `.mk` files).
1. Delete hidden `.settings` folder.
1. Delete the `.project` and `.cproject` files.
1. Open your `.ioc` file in the standalone **STM32CubeMX** application.
1. Click **Generate Code** at the top right.



Project Properties -> C/C++ General -> Preprocessor Include Paths, Macros etc. -> Entries tab

left panel: Languages GNU C

right panel: select `CDT User Setting Entries` click `Add...`, in dropdown, select `Preprocessor Macro`

Name: _Static_assert(a,b)

Value: leave blank

Treat as built-in: leave unchecked.

click ok.
