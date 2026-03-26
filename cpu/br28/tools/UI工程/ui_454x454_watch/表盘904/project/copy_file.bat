@echo off

..\..\..\UITools\style_table -stylefile project.bin -prj 0x1
copy .\project.bin ..\..\..\..\ui_resource\watch904.sty
copy .\result.bin  ..\..\..\..\ui_resource\watch904.res
copy .\result.str  ..\..\..\..\ui_resource\watch904.str
copy .\version.txt  ..\..\..\..\ui_resource\watch904.json
..\..\..\UITools\redefined -infile ename.h -prefix watch904 -outfile rename.h
if exist "..\..\..\..\..\..\..\apps\watch\include\ui\" (
    copy .\rename.h     ..\..\..\..\..\..\..\apps\watch\include\ui\style_watch904.h
)

exit
