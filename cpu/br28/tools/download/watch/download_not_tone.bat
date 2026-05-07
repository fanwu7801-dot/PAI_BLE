@echo off

@echo ********************************************************************************
@echo            SDK BR28 PACKAGE (NOT TONE)
@echo ********************************************************************************
@echo %date% %time%

cd %~dp0
set OUT_DIR=..\..\..\..\..\ufw_bin
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

mkdir res.safe.ori 2>nul
mkdir ui_upgrade 2>nul

copy ..\..\script.ver .
copy ..\..\uboot.boot .
copy ..\..\ota.bin .
copy ..\..\cfg_tool.bin .
copy ..\..\app.bin .
copy ..\..\br28loader.bin .
copy ..\..\user_api.bin .
copy ..\..\isd_config.ini .
copy ..\..\p11_code.bin .
copy ..\..\default.key .
copy ..\..\json.txt .
copy ..\..\eq_cfg_hw.bin .
copy ..\..\flash_params.bin .

copy /Y isd_config.ini isd_config_full.ini
powershell -NoProfile -ExecutionPolicy Bypass -Command "(Get-Content 'isd_config_full.ini') | ForEach-Object { if ($_ -match '^TONEFAT_FILE') { '; ' + $_ } elseif ($_ -match '^TONEFAT_OPT') { 'TONEFAT_OPT = 0;' } else { $_ } } | Set-Content 'isd_config.ini' -Encoding ASCII"
IF %ERRORLEVEL% NEQ 0 goto exit_point

cd ..\..\ui_resource
copy *.* ..\download\watch

cd %~dp0

cd ..\..\ui_upgrade
copy *.* ..\download\watch\ui_upgrade
cd %~dp0

..\..\json_to_res.exe json.txt
..\..\md5sum.exe app.bin md5.bin
set /p "themd5=" < "md5.bin"

..\..\packres.exe -keep-suffix-case JL.sty JL.res JL.str -n res -o JL
..\..\packres.exe -keep-suffix-case sidebar.sty sidebar.res sidebar.str sidebar.tab -n res -o sidebar
..\..\packres.exe -keep-suffix-case watch.sty watch.res watch.str watch.view watch.json -n res -o watch
..\..\packres.exe -keep-suffix-case watch1.sty watch1.res watch1.str watch1.view watch1.json -n res -o watch1
..\..\packres.exe -keep-suffix-case watch2.sty watch2.res watch2.str watch2.view watch2.json -n res -o watch2
..\..\packres.exe -keep-suffix-case watch3.sty watch3.res watch3.str watch3.view watch3.json -n res -o watch3
..\..\packres.exe -keep-suffix-case watch4.sty watch4.res watch4.str watch4.view watch4.json -n res -o watch4
..\..\packres.exe -keep-suffix-case watch5.sty watch5.res watch5.str watch5.view watch5.json -n res -o watch5
..\..\packres.exe -keep-suffix-case F_ASCII.PIX F_GB2312.PIX F_GB2312.TAB ascii.res -n res -o font

set CHIPKEY=default.key

..\..\fat_comm.exe -pad-backup2 -force-align-fat -out new_res.bin -image-size 16 -filelist JL sidebar watch watch1 watch2 watch3 watch4 watch5 font -remove-empty -remove-bpb -mark-bad-after 0xfe0000 -key %CHIPKEY% -address 0
IF %ERRORLEVEL% NEQ 0 goto exit_point

@rem not_tone: remove tone.cfg from -res and disable TONEFAT in isd_config.ini
@rem keep no -format to avoid accidental erase
..\..\isd_download.exe -tonorflash -dev br28 -boot 0x120000 -div8 -wait 300 -uboot uboot.boot -app app.bin cfg_tool.bin -res ui_upgrade p11_code.bin config.dat eq_cfg_hw.bin -flash-params flash_params.bin -uboot_compress -key %CHIPKEY% -ex_api_bin user_api.bin
IF NOT EXIST jl_isd.fw (
    @echo [ERROR] isd_download failed: jl_isd.fw not generated
    goto exit_point
)

@rem ota.bin is already included by isd_config.ini [FW_ADDITIONAL]
..\..\fw_add.exe -noenc -fw jl_isd.fw -add script.ver -out jl_isd.fw
IF %ERRORLEVEL% NEQ 0 goto exit_point

..\..\ufw_maker.exe -fw_to_ufw jl_isd.fw
IF %ERRORLEVEL% NEQ 0 goto exit_point

copy /Y jl_isd.ufw update_not_tone.ufw
copy /Y update_not_tone.ufw "%OUT_DIR%\update_not_tone.ufw"

@rem ?????????????????????
copy /Y update_not_tone.ufw update.ufw

if exist db_update_data.bin (
    copy /Y db_update_data.bin update_not_tone.bin
    copy /Y update_not_tone.bin "%OUT_DIR%\update_not_tone.bin"
) else (
    @echo [WARN] db_update_data.bin not found, skip update_not_tone.bin
)

if exist *.mp3 del *.mp3
if exist *.PIX del *.PIX
if exist *.TAB del *.TAB
if exist *.res del *.res
if exist *.sty del *.sty
if exist *.str del *.str
if exist *.anim del *.anim
if exist *.view del *.view
if exist *.json del *.json

:exit_point
if exist isd_config_full.ini (
    copy /Y isd_config_full.ini isd_config.ini >nul
    del isd_config_full.ini
)
ping /n 2 127.1>nul
IF EXIST null del null
