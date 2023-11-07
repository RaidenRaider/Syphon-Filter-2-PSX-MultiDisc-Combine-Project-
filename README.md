# Syphon-Filter-2-PSX-MultiDisc-Combine-Project-
A Project to combine disc 1 and disc 2 of Syphon Filter 2 into one bin/cue file.

Originally created by "Shalma" on https://www.ngemu.com/threads/disc-combining-kits.118402.

Syphon Filter 2 [SCUS-94451 / 94492]

This will combine your 2 game CDs into 1 disc.
Tested only with ePSXe 1.6.0 and ePSXe 1.7.0

WARNING: You'll need ~4GB to finish the operation

Tested versions:
- 2000-02-16  Original (NTSC-U)
- 2000-02-16  Greatest Hits (NTSC-U)
  
Not working versions:
- Original  (PAL-E)
- Platinum  (PAL-E)
- Original  (PAL-F)
- Platinum  (PAL-F)
- Original (EDC) (PAL-G)
- Original (No EDC) (PAL-G)
- Original  (PAL-I)
- Platinum  (PAL-I)
- Original  (PAL-S)
- Platinum  (PAL-S)

I REQUIRE HELP TO MAKE THIS WORK FOR THE PAL VERSIONS OF SYPHON FILTER 2.

README for NTSC-U versions of Syphon Filter 2:

0. Rip your CD images to bin/cue.
   Rename them syphon2-1.bin, syphon2-2.bin
   If you used Clone CD, rename the img to bin

1. Apply any PPF patches to your bin/cue
- They probably won't work when the new image is created

2. Run extract.bat
- This will rip the files out of your discs

3. Run patch\cd2dvd.bat
- Removes disc swapping

4. Move the contents of CD2 to folder 'merge' (overwrite all)
   Move the contents of CD1 to folder 'merge' (overwrite all)
- File merge\___merge.txt has a pre-built CD tree

5. Run merge\build.bat
- This will create your new disc, ~1.36GB
