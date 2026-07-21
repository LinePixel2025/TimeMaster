# Time Master - launch script
$QtDir = "D:\AICOP\requirements\QT6"
$MingwDir = "$QtDir\Tools\mingw1310_64"

$env:PATH = "$MingwDir\bin;$QtDir\6.11.1\mingw_64\bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$QtDir\6.11.1\mingw_64\plugins"

& ".\build\src\TimeMaster.exe"
