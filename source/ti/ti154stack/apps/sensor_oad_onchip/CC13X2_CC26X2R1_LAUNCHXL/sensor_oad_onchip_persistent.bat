REM "%1 = TARGET_BPATH"

ielftool.exe --ihex --verbose "%1.out" "%1.hex" 
