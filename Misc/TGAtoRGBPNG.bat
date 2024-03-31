@ECHO OFF
FOR /R %1 %%f IN (*.tga) DO (
	"C:\Program Files\ImageMagick\convert.exe" "%%~f" png24:"%%~dpnf.png"
)