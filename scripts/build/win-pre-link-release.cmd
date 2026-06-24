echo 'Copy dynamic libs in the release binary folder...'

if not exist release mkdir release
copy /Y ..\deps\quazip-1.7.1\install\bin\libquazip1-qt6.dll release
