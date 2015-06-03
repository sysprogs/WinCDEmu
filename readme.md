WinCDEmu Build Instructions
===========================

0. Checkout WinCDEmu (https://github.com/sysprogs/WinCDEmu), BazisLib (https://github.com/sysprogs/BazisLib) and STLPort-kernel (https://github.com/sysprogs/stlport-kernel) to a common directory.
0. Set the BZSLIB_PATH environment variable to point at the BazisLib directory
0. Set the STLPORT_PATH environment variable to point at the STLPort directory
0. Install WDK 7.x and set the WDK7PATH environment variable to point at it
0. Download/unpack WTL 8.0 and set WTL_PATH to point at it
0. Open WinCDEmu.sln in Visual Studio 2010-2015
0. Build the kernel-mode release configurations and then the user-mode release configurations
0. The binaries will be saved to AllModules