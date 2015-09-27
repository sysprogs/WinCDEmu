REM @echo off

call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat"
msbuild /m ..\WinCDEmu.sln /property:Platform=Win32 /property:Configuration="Release (Kernel-mode)" || goto error
msbuild /m ..\WinCDEmu.sln /property:Platform=x64 /property:Configuration="Release (Kernel-mode)" || goto error
msbuild /m ..\WinCDEmu.sln /property:Platform=Win32 /property:Configuration="Release (User-mode)" || goto error
msbuild /m ..\WinCDEmu.sln /property:Platform=x64 /property:Configuration="Release (User-mode)" || goto error

msbuild ..\InstallerStub\InstallerStub.sln /property:Platform=Win32 /property:Configuration="Release" || goto error
mkdir drivers
mkdir app
mkdir app\langfiles
xcopy /Y /S ..\AllModules\Drivers Drivers
xcopy /Y /S ..\AllModules\App App
xcopy /Y /S Langfiles App\Langfiles

del app\*.pdb
del app\x86\*.pdb
del app\x64\*.pdb
del app\x86\*.lib
del app\x64\*.lib
del app\x86\*.exp
del app\x64\*.exp
del drivers\x86\*.pdb
del drivers\x64\*.pdb
copy /y ..\redist\mkisofs.exe app\mkisofs.exe

if not exist ..\AllModules\InstallerStub.exe goto error
call ..\..\..\svn\utils\upx ..\AllModules\InstallerStub.exe

cipher /d /a /s:.

call ..\..\..\svn\utils\mkcat.bat drivers
call ..\..\..\svn\utils\sign_r.bat drivers\BazisVirtualCDBus.cat

call ..\..\..\svn\utils\ssibuilder VirtualCD.xit WinCDEmu-4.1.exe /linkcabs /nopause /exestub:..\AllModules\InstallerStub.exe
cipher /d /a *.exe
call ..\..\..\svn\utils\sign_r.bat WinCDEmu-4.1.exe
goto end
:error
echo Build failed!
pause

:end
pause