<#############
Object:  CreateImageFromCharacters.ps1
Version: 1.3.UW
Date:    2024-04-14
Author:  Draxinusom

Creates a PNG image of a string from character images using ImageMagick

Intended for use with older game fonts that are image based.
Requires the font's characters to be extracted as images of the same type
and named their ASCII code value (i.e. a=097) padded with 0 to a length of 3
Example: 068.bmp for the letter D / 032.bmp for a space

Note:  ImageMagick required - Update $ImageMagickPath variable below if necessary
       This will not handle fonts with gradiants/shading correctly
       Trying to invert a straight B/W font will leave the output completely transparent

Syntax:  CreateImageFromCharacters.ps1
           InString                                          String to generate an image of
           OutFile
           FontPath
           TargetColor <Optional/Default=FFFFFF (White)>     Target color for the character in the output image
           CharacterColor <Optional/Default=FFFFFF (White)>  Color of the source characters
           BackgroundColor <Optional/Default=000000 (Black)> Background of the source characters
           CharImageType <Optional/Default=BMP>              File type for source characters - Output is always PNG
#############>
param(
	[Parameter(Mandatory=$true,HelpMessage="String to create required.")]
	[string]$InString,
	[Parameter(Mandatory=$true,HelpMessage="Output file required.")]
	[string]$OutFile,
	[Parameter(Mandatory=$true,HelpMessage="Font character images location required.")]
	[string]$FontPath,
	[Parameter(Mandatory=$false,HelpMessage="Output text color (hex).")]
	[string]$TargetColor = 'FFFFFF',
	[Parameter(Mandatory=$false,HelpMessage="Input character color (hex).")]
#	[string]$CharacterColor = 'FFFFFF',		# Changing default for UWXtract
	[string]$CharacterColor = 'FCFCFC',
	[Parameter(Mandatory=$false,HelpMessage="Input character background (hex).")]
#	[string]$BackgroundColor = '000000',	# Changing default for UWXtract
	[string]$BackgroundColor = '000004',	# UW1's background is this / UW2's is already 000000 (black) so doesn't matter unless you try to change the text to 000000
	[Parameter(Mandatory=$false,HelpMessage="Character image type.")]
#	[string]$CharImageType = 'BMP'			# Changing default for UWXtract
	[string]$CharImageType = 'TGA'
)
# Hardcoded paths
$ImageMagickPath = "C:\Program Files\ImageMagick"
$WorkingFolder = $env:TEMP + "\CharImageTemp"

# Validate parameters
# Font character image path
if(!(Test-Path "$FontPath")) {
	Write-Host "Font folder not found. Aborting." -ForegroundColor Red
	Return
}
if(!(Test-Path "$FontPath\*.$CharImageType")) {
	Write-Host "No $($CharImageType.ToUpper()) character images found in font folder. Aborting." -ForegroundColor Red
	Return
}
# Target text color
if($TargetColor.length -ne 6 -or !($TargetColor -match "[0123456789ABCDEF]{6}")) {
	Write-Host "Invalid target color specified. Aborting." -ForegroundColor Red
	Return
}
# Character text color
if($CharacterColor.length -ne 6 -or !($CharacterColor -match "[0123456789ABCDEF]{6}")) {
	Write-Host "Invalid character color specified. Aborting." -ForegroundColor Red
	Return
}
# Background color
if($BackgroundColor.length -ne 6 -or !($BackgroundColor -match "[0123456789ABCDEF]{6}")) {
	Write-Host "Invalid background color specified. Aborting." -ForegroundColor Red
	Return
}
# ImageMagick
if(!(Test-Path "$ImageMagickPath\magick.exe" -PathType leaf)) {
	Write-Host "ImageMagick not found. Aborting." -ForegroundColor Red
	Return
}
# OutFile
if(!(Test-Path "$OutFile" -IsValid -PathType leaf)) {
	Write-Host "Specified output file path is invalid. Aborting." -ForegroundColor Red
	Return
}

# Set alpha to use -- Normally setting black to alpha but need to switch if current character color is black
$TargetAlpha = if($CharacterColor -eq "000000") {"FFFFFF"} else {"000000"}

# Remove temp folder if exists -- May have leftover files that will get added to output
if(Test-Path -Path "$WorkingFolder") {
	Remove-Item $WorkingFolder -Recurse -Force
}

# Create temp folder to store character images to be concatenated
$null = mkdir $WorkingFolder

# Split string into an array of characters
$Characters = $InString.ToCharArray()

# Then loop array and copy target character renamed to character position in string
$CharPos = 0
foreach ($c in $Characters) {
	$InChar = '{0:d3}' -f [byte][char]$c
	$OutChar = '{0:d4}' -f $CharPos
	# Do try/catch as not all characters may be available in font (most image fonts are limited to alpha, numeric, and a few punctuations)
	try {
		Copy-Item "$($FontPath)\$($InChar).$($CharImageType)" "$($WorkingFolder)\$($OutChar).$($CharImageType)" -ErrorAction Stop | Out-Null
	}
	catch {
		Write-Host "Target character ""$($c)"" ($($InChar)) at position $($CharPos) in string not available for this font. Aborting." -ForegroundColor Red
	}
	$CharPos += 1
}

# Create output folder if not exists
if(!(Test-Path $OutFile.Substring(0, $OutFile.LastIndexOf('\')))) {
	$null = mkdir $OutFile.Substring(0, $OutFile.LastIndexOf('\'))
}

# Create output image
$ImgOut = Start-Process -FilePath "$($ImageMagickPath)\magick.exe" -NoNewWindow -PassThru `
	-ArgumentList `
		"`"$($WorkingFolder)\*.$($CharImageType)`"",				# Input images
		"+append",													# Concatenates the images horizontally (-append will stack them vertically)
		"-fill #$($TargetAlpha)", "-opaque #$($BackgroundColor)",	# Remap background to target alpha color
		"-transparent #$($TargetAlpha)",							# Then set target alpha to transparent
		"-fill #$($TargetColor)", "-opaque #$($CharacterColor)",	# Remap character's color to target
		"png32:`"$($OutFile)`""										# Force output to 32-bit png

$ImgOut | Wait-Process

# Remove temp folder
Remove-Item $WorkingFolder -Recurse -Force

Write-Host "Text image exported to ""$($OutFile)""."