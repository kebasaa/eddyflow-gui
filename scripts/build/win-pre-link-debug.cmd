echo 'Copy dynamic libs in the debug binary folder...'

if not exist debug mkdir debug
copy /Y ..\deps\quazip-1.7.1\install\bin\libquazip1-qt6.dll debug
