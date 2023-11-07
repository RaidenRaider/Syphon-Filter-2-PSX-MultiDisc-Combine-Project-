SET FILE="syphon2.bin"

mode2cdmaker -s -paramfile ___merge.txt
mode2cdmaker -write_form2 0 license.bin %FILE%
mode2cdmaker -ps1_list %FILE%
pause