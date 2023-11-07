Syphon Filter 2 [SCUS-94451 / 94492]

This will combine your 2 game CDs into 1 disc
Tested only with ePSXe 1.7.0

WARNING:
You'll need ~4GB to finish the operation


Tested versions:
- 2000-02-16  Original
- 2000-02-16  Greatest Hits



0. Rip your CD images to bin/cue.
   Rename them syphon2-1.bin, syphon2-2.bin

   If you used Clone CD, rename the img to bin

0a. Apply any PPF patches to your bin/cue
- They probably won't work when the new image is created

1. Run extract.bat
- This will rip the files out of your discs

2. Run patch\cd2dvd.bat
- Removes disc swapping

3. Move the contents of CD2 to folder 'merge' (overwrite all)
   Move the contents of CD1 to folder 'merge' (overwrite all)
- File merge\___merge.txt has a pre-built CD tree

4. Run merge\build.bat
- This will create your new disc, ~1.36GB
