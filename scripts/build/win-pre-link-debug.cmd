echo 'Copy dynamic libs in the debug binary folder...'

if not exist debug mkdir debug
copy /Y ..\deps\quazip-1.7.1\install\bin\libquazip1-qt6.dll debug

echo 'Copy ancillary template files in the debug binary folder...'

if not exist debug\file-templates mkdir debug\file-templates
copy /Y "%~dp0..\..\file-templates\*.txt" debug\file-templates
