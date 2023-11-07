#include <stdio.h>
#include <string.h>

FILE *in, *out, *param;
char line[4096], file_name[4096], name2[4096];
int target_write;

#define BLOCKS2MIN(x) ((x)/(75*60))
#define BLOCKS2SEC(x) (((x)/75)%60)
#define BLOCKS2FRA(x) ((x)%75)

#define LBA2MIN(x) (BLOCKS2MIN(x+150))
#define LBA2SEC(x) (BLOCKS2SEC(x+150))
#define LBA2FRA(x) (BLOCKS2FRA(x+150))

#define MSF2LBA(m,s,f) (((((m)*60*75)+(s)*75)+(f))-150)

#define btoi(b)		((b)/16*10 + (b)%16)		/* BCD to u_char */
#define itob(i)		((i)/10*16 + (i)%10)		/* u_char to BCD */


int main( int argc, char **argv )
{
	sprintf( file_name, "lba\\%s", argv[1] );

	in = fopen( "lba\\___test-lba.txt", "r" );
	param = fopen( file_name, "r" );
	out = fopen( "lba\\cd2dvd-lba.txt", "w" );


	while(1)
	{
		fgets( line, 4096, param );
		if( feof(param) ) break;


		name2[0] = 0;
		sscanf( line, "%s", name2 );

		if( strcmp( name2, "insert_dummy" ) == 0 )
		{
			fprintf( out, "; DUMMY\n" );
			fprintf( out, "$%X\n", target_write );
			target_write += 4;

			fprintf( out, "00000000\n\n" );

			continue;
		}


		if( strcmp( name2, "set_target" ) == 0 )
		{
			sscanf( line, "set_target %X\n", &target_write );
			continue;
		}


		if( strcmp( name2, "write_time" ) == 0 )
		{
			sscanf( line, "write_time %s\n", file_name );

			int start, end;
			while(1)
			{
				if( feof(in) )
				{
					fseek( in, 0, SEEK_SET );
					continue;
				}
				fgets( line, 4096, in );

				sscanf( line, "%X-%X [%s]", &start, &end, name2 );
				end--;
				name2[ strlen(name2)-1 ] = 0;

				// match
				if( strcmp( file_name, name2 ) == 0 )
				{
					fprintf( out, "; %s\n", name2 );
					fprintf( out, "$%X\n", target_write );
					target_write += 4;

					fprintf( out, "%02X%02X%02X00\n\n",
						itob( LBA2MIN( start ) & 0xff ),
						itob( LBA2SEC( start ) & 0xff ),
						itob( LBA2FRA( start ) & 0xff ) );

					break;
				}
			}

			continue;
		}


		if( strcmp( name2, "write_addr" ) == 0 )
		{
			sscanf( line, "write_addr %s\n", file_name );

			int start, end;
			while(1)
			{
				if( feof(in) )
				{
					fseek( in, 0, SEEK_SET );
					continue;
				}
				fgets( line, 4096, in );

				sscanf( line, "%X-%X [%s]", &start, &end, name2 );
				end--;
				name2[ strlen(name2)-1 ] = 0;

				// match
				if( strcmp( file_name, name2 ) == 0 )
				{
					fprintf( out, "; %s\n", name2 );
					fprintf( out, "$%X\n", target_write );
					target_write += 4;

					fprintf( out, "%02X%02X%02X00\n\n",
						(start>>0) & 0xff,
						(start>>8) & 0xff,
						(start>>16) & 0xff );

					break;
				}
			}
		}
	}


	if(in) fclose(in);
	if(param) fclose(param);
	if(out) fclose(out);

	return 0;
}