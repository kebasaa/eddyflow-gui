echo 'Copy dynamic libs in the release binary folder...'

if not exist release mkdir release
copy /Y ..\deps\quazip-1.7.1\install\bin\libquazip1-qt6.dll release

echo 'Copy ancillary template files in the release binary folder...'

if not exist release\file-templates mkdir release\file-templates
copy /Y "%~dp0..\..\file-templates\*.txt" release\file-templates
