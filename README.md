# Ultima Underworld Extractor (UWXtract)
Ultima Underworld Extractor is a command line tool designed to extract and decode the data for Ultima Underworld 1 and 2 (referred to as UW1 or UW2).

## Download
https://github.com/Draxinusom/UWXtract/releases

This package includes all binaries required.  There is no installation, registry updates, etc.  Just download and run.

## Prerequisites
- Windows only
- Original game files from UW1 and/or UW2

All development and testing was done with the GOG version of the game.  While it _should_ work fine with other older versions, I can't say for certain.  My guess is the MDL extract is the most likely to fail and can be worked around by specifying each data type instead of executing all (*).

Note:  
For GOG version, the ISO file (game.gog - you can temporarily rename it to iso) must be mounted and the game folder specified or you can extract it and run from there.

## Usage
**Syntax:**
```shell
	UWXtract [type] [Game path/Optional] [Output path/Optional]
```

**Example:**
```shell
	UWXtract.exe * "C:\Games\Ultima Underworld\UW1" "C:\Users\Draxinusom\Desktop\UW1Xtract"
```
The above example will export all data types for UW1 to the specified folder on the desktop.

Alternatively, UWXtract.exe and supporting files may be placed in and run from the base game folder (i.e. where UW.exe/UW2.exe is located) and directly run from there with only the extraction type specified:

**Example:**
```shell
	UWXtract.exe TR
```
This will create a folder called UWXtract in the game folder and extract the terrain image files there

Note:  
When the * option is used (with or without the output path), sub folders are created for each data type so as not to make a mess.  When a specific type is specified, it is extracted directly there.

## Data Extracted
### Images
| Type | Description | UW1 Files | UW2 Files |
|------|-------------|-----------|-----------|
|BYT|Various fullscreen images|DATA\\*.BYT | CUTS\\*.BYT / DATA\BYT.ARK|
|CRIT|Critter (Monster/NPC) animation frames|CRIT\\CR*|CRIT\\CR*|
|FONT|Font characters|DATA\\*.SYS|DATA\\*.SYS|
|GR|Objects/UI Elements/Etc|DATA\\*.GR|DATA\\*.GR|
|TR|Floor/Wall textures|DATA\\*.TR|DATA\\*.TR|

Images are extracted as TGA files.  I've included two batch files (you'll likely need to adjust the paths) to convert the files to PNG:
- TGAtoIndexPNG.bat:  Convert to PNG keeping the image's color map exactly the same.  Requires [FFMPEG](https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full.7z).
- TGAtoRGBPNG.bat:  Convert to a standard 24-bit (no alpha) RGB PNG.  Requires [ImageMagick](https://imagemagick.org/script/download.php#windows).

If you don't know the difference or why you would prefer one over the other, use TGAtoRGBPNG.bat.

Note:  
Do **_not_** use ImageMagick if you want to keep the converted files as indexed.  ImageMagick **_will_** alter the color map.

### Data
| Type | Description | UW1 Files | UW2 Files |
|------|-------------|-----------|-----------|
|DAT|Various data/object type details|Described below|Described below|
|LEV|Level archive|DATA\\LEV.ARK / SAVE#\\LEV.ARK|DATA\\LEV.ARK / SAVE#\\LEV.ARK|
|MDL|3D model details|UW.EXE|UW2.EXE|
|PAK|Game strings|DATA\\STRINGS.PAK|DATA\\STRINGS.PAK|
|PAL|Image color palettes|Described below|Described below|
|SCD|SCD archive|N/A|DATA\\SCD.ARK / SAVE#\\SCD.ARK|
|SAV|Game saves|DATA\\PLAYER.DAT / SAVE#\\PLAYER.DAT|N/A|

#### DAT
| File | Description | Game |
|------|-------------|------|
|DATA\CMB.DAT|Item combinations|Both|
|DATA\COMOBJ.DAT|Common object properties|Both|
|DATA\DL.DAT|Level specific lighting|UW2|
|DATA\OBJECTS.DAT|Object class specific properties - contains multiple datasets|Both|
|DATA\SHADES.DAT|Contains parameters around light levels - distance to start darkening the screen|Both|
|DATA\SKILLS.DAT|Character creation class starting attributes and available skills|Both|
|DATA\SOUNDS.DAT|Guessed to be midi commands but values don't align with any standards commands I can find|Both|
|DATA\TERRAIN.DAT|Contains the type of terrain for each - i.e. lava/normal/water/etc.|Both|
|DATA\WEAP.DAT|Offsets for placing the attack frames from WEAP.GR on the screen for each type|UW2|
|DATA\WEAPONS.DAT|Offsets for placing the attack frames from WEAPONS.GR on the screen for each type|UW1|

#### PAL
| File | Description | Game |
|------|-------------|------|
|DATA\ALLPALS.DAT|Auxillary palettes - 16 color palette using colors for PAL.DAT palette 0|Both|
|DATA\LIGHT.DAT|Light levels - full palette replacement for the view screen based on the light level|Both|
|DATA\MONO.DAT|Grayscale - full palette replace for the view screen for invisibility|Both|
|DATA\PALS.DAT|Primary palettes - full 256 color palettes - 0 is most utilized|Both|
|DATA\WEAP.CM|Attack frame palettes|UW2|
|DATA\WEAPONS.CM|Attack frame palettes|UW1|

Note:  
While a few data extracts are complete, most contain (at least some) data whose purpose is unclear, unknown, or a best guess.  In those cases I have generally left the data as a hexadecimal byte value where the entire byte is unknown and where part of the byte is known I return each individual bit for the remaining data.

### LEV/SAV/SCD
For these data types, you can specify the specific save to extract or the original data version by adding the save number or "d" to the type parameter (i.e. SAV2).  Additionally, you can specify all with * (SCD*).

If none is specified, LEV & SCD will behave as if you specified the data version (i.e. LEVD) and SAV will extract SAVE1 (i.e. SAV1).

## Build
This project was originally created as a modified version of the "hacking" tools included in [UnderworldAdventures](https://github.com/vividos/UnderworldAdventures) (UA) and still relies on code from that project.  While I intend to work through and extract out just the components this project requires to make it standalone, as of now, you will also need the source for that.

I probably should (but probably won't) test this process out but I think all you should need to do is:
1.  Download and extract the source for [UA](https://github.com/vividos/UnderworldAdventures/releases/download/version-0.10.0-pina-colada/uwadv-0.10.0-pina-colada-windows.zip)
2.  Download the source for [UWXtract](https://github.com/Draxinusom/UWXtract/releases/download/latest.zip) and extract to the uwadv folder in the UA source
3.  Open uwadv.sln in a text editor and find/replace all instances of "hacking" with "UWXtract"
4.  Open the solution in Visual Studio and build
5.  Try to figure out and fix all the compile errors
6.  Victory

## D.A.Q.
Just don't.

## History
UWXtract was originally created while I was looking for an extract of the images from UW2.  There are plenty of dumps from UW1 (though generally incomplete) but was unable to find one for UW2.  [Underworld-Editor](https://github.com/hankmorgan/Underworld-Editor) would extract them, but only one at a time and it errored on some files.  While looking at the UnderworldAdventures project, I found the folder with their "hacking" tools which looked to have a function that would do what I wanted, though that required compiling it myself.

I'm a reasonably skilled SQL (and occasional mediocre PowerShell) developer, not a C/C++ developer.  At all.  My knowledge of it was being aware it exists.  Still, I was able to get the solution setup in Visual Studio and get it to compile and... it didn't work.  With a little bit of poking around I was able to figure out what was wrong (mostly path hardcoding) and get it working right and exported what I was originally looking for.

After this, I decided this could be a fun little side project to try to learn C++ a bit and so UWXtract was born.  My original intent was just to get all the various functions working with a good wrapper to call them but then scope creep happened and now there's this mess.

About three quarters of the way through developing v1, I learned the vast majority all the functions/syntax/etc I was using was actually C, not C++.  Fortunately, I possess the boldness and upper body strength to pick up and relocate a goalpost whenever and to wherever I want, enabling me to claim success in my aim to write a program in a language I didn't know that wasn't the one I thought I was writing it in.  Mostly.

It's pretty close though.

## Credits
This project began as a simple wrapper for the "hacking" tools created by the [Underworld Adventures](https://vividos.github.io/UnderworldAdventures/) Team.  While much of the original code from them has been heavily altered or completely replaced, quite a bit still remains (string loading/decoding and UW2 decompression are essentially untouched) and all credit goes to them for that.  Even the parts that were rewritten/replaced were vital as that's what I used to learn how to write (a little bit of) ~~C++~~ C.  As importantly, I must thank them for the work they did in documenting the internals of UW1/UW2, without which none of this would have been possible.  I have spent a ridiculous amount of time reading their [uw-formats.txt](https://github.com/vividos/UnderworldAdventures/blob/main/uwadv/docs/uw-formats.txt) document.

Additionally, much credit must be given to hankmorgan and his [reverse engineering documentation](https://github.com/hankmorgan/UWReverseEngineering) as part his work on [UnderworldExporter](https://github.com/hankmorgan/UnderworldExporter).  His work has been invaluable in flushing out much of the details in the data files, saves, and level archives (particularly for UW2).  UnderworldExporter is an active project (at least as of March 2024) and is definitely worth keeping an eye on if you're interested in Ultima Underworld.  Despite it not actually exporting anything :)

