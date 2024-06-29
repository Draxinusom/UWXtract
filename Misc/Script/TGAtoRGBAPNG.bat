@ECHO OFF
FOR /R %1 %%f IN (*.tga) DO (
	"C:\Program Files\ImageMagick\convert.exe" "%%~f" -type TrueColorAlpha png32:"%%~dpnf.png"
)