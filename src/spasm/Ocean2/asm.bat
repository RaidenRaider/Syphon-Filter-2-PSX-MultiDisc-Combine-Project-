del *.bin

SET FILE_1=slus-cdrom
SET FILE_2=slus-swap

spasm -b %FILE_1%1.txt  %FILE_1%1.bin
spasm -b %FILE_1%1a.txt %FILE_1%1a.bin

spasm -b %FILE_2%1.txt  %FILE_2%1.bin
spasm -b %FILE_2%1a.txt %FILE_2%1a.bin
spasm -b %FILE_2%2.txt  %FILE_2%2.bin
spasm -b %FILE_2%2a.txt %FILE_2%2a.bin

pause
