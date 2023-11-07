del *.bin

SET FILE_1=slus-swap

spasm -b %file_1%1.txt  %file_1%1.bin
spasm -b %file_1%1a.txt %file_1%1a.bin

pause
