del *.bin

SET FILE_1=slus

spasm -b %FILE_1%1.txt %FILE_1%1.bin
spasm -b %FILE_1%1a.txt %FILE_1%1a.bin

pause
