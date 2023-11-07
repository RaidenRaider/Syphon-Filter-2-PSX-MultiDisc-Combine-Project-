cd ..\merge
mode2cdmaker -s -test_write 16 -paramfile ___merge.txt

cd ..\patch
move ..\merge\___test-lba.txt lba

lba_script ff8-img.txt
move lba\cd2dvd-lba.txt patch\ff8-img-lba.txt

move "..\merge\SLUS_008.92" ".\"
cd2dvd_patcher ff8-img-lba.txt SLUS_008.92
move "SLUS_008.92" "..\merge"
