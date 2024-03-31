@ECHO OFF
FOR /R %1 %%f IN (*.tga) DO (
	"C:\Program Files\FFMPEG\bin\ffmpeg.exe" -v 0 -hide_banner -y -i "%%~f" "%%~dpnf.png">NUL
)