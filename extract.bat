SET FILE1="syphon2-1.bin"
SET FILE2="syphon2-2.bin"


rmdir /Q /S cd1
rmdir /Q /S cd2
rmdir /Q /S out_cd


mode2cdmaker -ps1_extract %FILE1%
ren out_cd cd1
ren cd1\___cd.txt ___cd1.txt

mode2cdmaker -read_form2 0 f %FILE1% license.bin
move license.bin cd1

mode2cdmaker -ps1_list %FILE1%
ren ___cd_lba.txt lba1.txt
move lba1.txt cd1



mode2cdmaker -ps1_extract %FILE2%
ren out_cd cd2
ren cd2\___cd.txt ___cd2.txt

mode2cdmaker -read_form2 0 f %FILE2% license.bin
move license.bin cd2

mode2cdmaker -ps1_list %FILE2%
ren ___cd_lba.txt lba2.txt
move lba2.txt cd2
