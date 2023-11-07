SET FILE_1=SCUS_944.51


move "..\cd1\%FILE_1%" ".\"
cd2dvd_patcher syphon2-scus-swap.txt %FILE_1% 0
move "%FILE_1%" "..\cd1"


ren "..\cd1\FOG" FO1
ren "..\cd2\FOG" FO2
