                 spASM version 0.2 by Silpheed of Hitmen
    
              The first release of my Playstation assembler


This assembler still has far to go before it's complete, but I am releasing
it now as I think its at a fairly usable state. 

Spasm recognises all machine instructions, and several pseudo-instructions
too.

These are the pseudo-instructions it knows of:
        b
        la
        li
        nop
        move
        subi
        subiu
        beqz
        bnez
        bal

Also you are allowed to use 'lw', 'sw', 'lb' etc.. with labels, instead of
the usual offset(register). Eg. the following are all ok:

        "lw t0, temp"
        "lb a1, 1234(gp)"
        "sh zero, $34(a1)"
        "sw a1, buffer+$100"
        "sb a2, $80100000"

It also recognises the following pseudo-pseudo-instructions :)

       - leaving out the destination register for an instruction
           eg. these produce the same code:

         "addi a0,a0,1"  and  "addi a0,1"
         "xor s1,s1,t1"  and  "xor s1,t1"

         etc...

       - putting an immediate value as the last argument to an instruction
         that expects a register instead.
           eg.
         
         "or a0,a1,1"  becomes  "ori a0,a1,1"
         "add sp,4"    becomes  "addi sp,sp,4"
         
         etc...

As you can see above, spasm recognises the register names like "zero", "a0",
"sp", "gp", "t9" etc. You can also use the numbered type by putting an 'r'
in front like this: "r01", "R9", "r31" etc.

The following directives are allowed:

         org
         include source_file
         incbin binary_file
         dcb
         db
         dh
         dw
         equ ( or "=" )
         align

The 'org' directive must come before any code in your source file, and is
only allowed once. If no org is found, the assembler will default to
$80010000.

The 'dcb' directive is used for allocating blocks of multiple bytes with
the same value - use "dcb number_of_bytes, value_of_bytes".

The 'align' directive is used to align code or data to word or half-word
boundaries... 'align 4' to make sure what follows is at a word-aligned
address, 'align 2' for a half-word aligned address.

The 'db' directive is also allowed to contain text, so things like this
are allowed:

Message db 7,"Hello World!",$0d,$A,0

The text must be enclosed in quotes (either ' or " is allowed).

The 'equ' directive is only allowed to represent numeric values, not text. 
This means thing like this are ok:

printf equ $3f

blah = start+$30

but not this:

newadd equ addiu


The 'include' and 'incbin' directives are used to include other files into
your source. Filenames should not have quotes around them. At the moment
there is a limit of 16 "include" files, but no limit on "incbin" files.
You can have "include" directives in already included source files too.

Numbers can be hex or decimal, use '$' to denote a hex number. Mathematical
expressions are allowed in most places, but only simple ones at the moment.
They can only be of the form "x op y" where 'op' is either +,-,&,|,<< or >>.

Symbols can be up to 32 characters long, and contain A-Z, a-z and '_'. They
can also have an optional ':' at the end. Symbol names are case sensitive,
instructions/directives and register names are not.

I think that's most of what needs to be said, please email me with any
bugs you find or suggestions for new features.

 - Silpheed/HITMEN


