@echo off
set x=hydraload

md build\chrome
cd chrome
"%ProgramFiles%/7-zip/7z.exe" a -tzip -x!*.svn "%x%.jar" * -r -mx=0
move "%x%.jar" ..\build\chrome
cd ..
copy install.* build
cd build
"%ProgramFiles%/7-zip/7z.exe" a -tzip -x!*.svn "%x%.xpi" * -r -mx=9
move "%x%.xpi" ..\
cd ..
rd build /s/q