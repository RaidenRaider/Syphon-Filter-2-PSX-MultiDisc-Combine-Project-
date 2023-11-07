del *.bin

SET FILE_1=swap
SET FILE_2=exp

spasm -b %FILE_1%1.txt %FILE_1%1.bin
spasm -b %FILE_1%1a.txt %FILE_1%1a.bin
spasm -b %FILE_1%2.txt %FILE_1%2.bin
spasm -b %FILE_1%3.txt %FILE_1%3.bin
spasm -b %FILE_1%4.txt %FILE_1%4.bin
spasm -b %FILE_1%4a.txt %FILE_1%4a.bin
spasm -b %FILE_1%5.txt %FILE_1%5.bin
spasm -b %FILE_1%5a.txt %FILE_1%5a.bin
spasm -b %FILE_1%6.txt %FILE_1%6.bin
spasm -b %FILE_1%6a.txt %FILE_1%6a.bin
spasm -b %FILE_1%7.txt %FILE_1%7.bin

spasm -b %FILE_2%1.txt %FILE_2%1.bin
spasm -b %FILE_2%1a.txt %FILE_2%1a.bin
spasm -b %FILE_2%2.txt %FILE_2%2.bin
spasm -b %FILE_2%2a.txt %FILE_2%2a.bin

pause
