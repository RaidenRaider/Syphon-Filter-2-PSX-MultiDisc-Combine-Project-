/*

    2003/02/01 - 1.5.1 - DeXT
    
    vcdisofs.c: fixed file names when using relative paths & ':' character only

    2002/10/13 - 1.5 - DeXT
    
    vcdisofs.c: each directory can now contain an unlimited number of files
                (it was previously limited to about 32 per directory)
    vcdisofs.c: added nested subdirectories support
    vcdisofs.c: better use of ISO track space (files are being recorded right
                after the ISO FS)
    vcdisofs.c: directory record buffers are now allocated at run-time
    vcdisofs.c: fixed international character set support for subdirs, too

    2002/08/26 - 1.4.2 - ML & DeXT
    
    vcdisofs.c: fixed character set for international file names

    2002/06/20 - 1.4 - DeXT
    
    vcdisofs.c: subdirs now work for form2 files, too
    vcdisofs.c: form2 original extension can now be kept
    vcdisofs.c: fixed max filename length calculation (was greater than allowed)
    vcdisofs.c: ISO building is now faster for form1 files (no need to reserve space)
    
    2002/05/23 - 1.3 - DeXT
    
    vcdisofs.c: added subdirectories support (for Form1 files only)
    vcdisofs.c: added ASCII character set support

    2002/05/17 - 1.2 - DeXT
    
    vcdisofs.c: added Form1 files support

    2002/05/06 - 1.2pre2 - DeXT
    
    vcdisofs.c: improved XA compatibility (added XA sig for every file/dir)
    vcdisofs.c: added long file names support (ISO-9660 Level 2)
    vcdisofs.c: fixed emtpy block in ISO track
    
    2002/05/04 - 1.2pre - Nic
    
    vcdisofs.c: M2F2 file extension is no longer fixed

    2002/04/18 - beta 1.1 - DeXT
    
    vcdisofs.c: lowered ISO track size to 302 sectors (~700 Kb)

    2002/04/17 - beta 1.0 - DeXT

    vcdisofs.c: fixed make_path_tables() to not add root files as dirs
    vcdisofs.c: DAT files are added to the root directory instead of MPEGAV
    vcdisofs.c: removed unneeded VCD stuff

*/

/*
 * Make a (simple) ISO 9660 filesystem for a Video CD
 *
 * Many things in this program are just taken from mkisofs
 *
 * Structure of the filesystem
 *
 *     Block      Content
 *
 *     0-15       empty
 *     16         Primary Volume descriptor
 *     17         End Volume descriptor
 *     18         Path table (Intel Byte order) (must fit in 1 Sector)
 *     19         Path table (Motorola Byte order) (must fit in 1 Sector)
 *     20         Root directory
 *     21  ...    Other directories
 *     150 ...    Files in directory VCD
 *     210 ...    Other files
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <time.h>
#include "defaults.h"
#include <sys/stat.h>

/* Path table stuff. Note that this is simplified and only valid
   if there are no subdirectories below the directories in the
   root directory and if the path tables fit into 1 sector */

#define PVD_EXTENT 16
#define EVD_EXTENT 17
#define PATH_TABLE_L_EXTENT 18
#define PATH_TABLE_M_EXTENT 20

#define ISO_DIR_SIZE 2048

static char path_table_l[2048];
static char path_table_m[2048];
static int  path_table_size;

static char zero2048[2048] = { 0, };
static char buff2048[2048];
static unsigned char buff2352[2352];
unsigned int ROOT_DIR_EXTENT = 22;

unsigned int dir_lba[256];
int dir_count;

extern void output_form1(int rec, unsigned char *data);

/* Various files on the VCD */

static char entries_file[2048];
static char info_file[2048] = {
  'V', 'I', 'D', 'E', 'O', '_', 'C', 'D',
    1,   1, '1', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ',   0,   1,   0,   1,   0,   0,
};

static struct tm *t;

#define BCD(x) ( ((x)/10)*16 + (x)%10 )

static unsigned short get_721( char *pnt )
{
	unsigned int i;

	i = (((unsigned char)pnt[0])<<0) +
			(((unsigned char)pnt[1])<<8);
	return i;
}

static unsigned int get_731( char *pnt )
{
	unsigned int i;

	i = (((unsigned char)pnt[0])<<0) +
			(((unsigned char)pnt[1])<<8) +
			(((unsigned char)pnt[2])<<16) +
			(((unsigned char)pnt[3])<<24);
	return i;
}

static void set_721( char *pnt, unsigned int i)
{
     pnt[0] = i & 0xff;
     pnt[1] = (i >> 8) &  0xff;
}

static void set_722( char *pnt, unsigned int i)
{
     pnt[0] = (i >> 8) &  0xff;
     pnt[1] = i & 0xff;
}

static void set_723( char *pnt, unsigned int i)
{
     pnt[3] = pnt[0] = i & 0xff;
     pnt[2] = pnt[1] = (i >> 8) &  0xff;
}

static void set_731( char *pnt, unsigned int i)
{
     pnt[0] = i & 0xff;
     pnt[1] = (i >> 8) &  0xff;
     pnt[2] = (i >> 16) &  0xff;
     pnt[3] = (i >> 24) &  0xff;
}

static void set_732( char *pnt, unsigned int i)
{
     pnt[3] = i & 0xff;
     pnt[2] = (i >> 8) &  0xff;
     pnt[1] = (i >> 16) &  0xff;
     pnt[0] = (i >> 24) &  0xff;
}

static void set_733( char *pnt, unsigned int i)
{
     pnt[7] = pnt[0] = i & 0xff;
     pnt[6] = pnt[1] = (i >> 8) &  0xff;
     pnt[5] = pnt[2] = (i >> 16) &  0xff;
     pnt[4] = pnt[3] = (i >> 24) &  0xff;
}

static void set_str( char *pnt, char *src, unsigned int len)
{
     int i, l;
     l = strlen(src);

     for(i=0;i<len;i++) pnt[i] = (i<l) ? src[i] : ' ';
}

void make_path_tables(dir_s **dir_entry, int num_dirs)
{
   int i, j, len;

   memset(path_table_l, 0, sizeof(path_table_l));
   memset(path_table_m, 0, sizeof(path_table_m));

   path_table_size = 0;
   
   for(i = 0; i < num_dirs; i++)
   {
      len = strlen(dir_entry[i]->name);
      if (len == 0) len = 1; // root dir

      if (path_table_size + 8 + (len & 1 ? len + 1 : len) > sizeof(path_table_l)) // avoid crossing path table boundary
         { fprintf(stderr,"Fatal Error: too many dirs\n"); exit(1); }

      path_table_l[path_table_size] = len;
      path_table_m[path_table_size] = len;
      path_table_size += 2;

      set_731(path_table_l + path_table_size, dir_entry[i]->extent);
      set_732(path_table_m + path_table_size, dir_entry[i]->extent);
      path_table_size += 4;

      set_721(path_table_l + path_table_size, dir_entry[i]->parent_num);
      set_722(path_table_m + path_table_size, dir_entry[i]->parent_num);
      path_table_size += 2;

      for(j=0; j<len; j++)
      {
         path_table_l[path_table_size] = dir_entry[i]->name[j];
         path_table_m[path_table_size] = dir_entry[i]->name[j];
         path_table_size++;
      }

      if(path_table_size & 1) path_table_size++;
   }
}

/*
void make_path_tables(dir_s **dir_entry)
{
   int i, j, len, extent, next_entry;
   unsigned char *root_dir_entry;

   memset(path_table_l, 0, sizeof(path_table_l));
   memset(path_table_m, 0, sizeof(path_table_m));

   path_table_size = 0;

   root_dir_entry = dir_entry[0]->record;

   for(i = 0; i < dir_entry[0]->len; i += next_entry)
   {
      next_entry = root_dir_entry[i]; // offset for next entry
      if (next_entry == 0) {
         next_entry = ISO_DIR_SIZE-(i%ISO_DIR_SIZE); // skip to next block boundary
         continue; }

      if (root_dir_entry[i+33]==1) continue; // skip .. in root
      if (root_dir_entry[i+25]==0) continue; // skip files

      len = root_dir_entry[i+32];

      if (path_table_size + 8 + (len & 1 ? len + 1 : len) > sizeof(path_table_l)) // avoid crossing path table boundary
         { fprintf(stderr,"Fatal Error: too many dirs\n"); exit(1); }

      path_table_l[path_table_size] = len;
      path_table_m[path_table_size] = len;
      path_table_size += 2;

      extent =  root_dir_entry[i+2]      +
               (root_dir_entry[i+3]<< 8) +
               (root_dir_entry[i+4]<<16) +
               (root_dir_entry[i+5]<<24);
      set_731(path_table_l + path_table_size, extent);
      set_732(path_table_m + path_table_size, extent);
      path_table_size += 4;

      set_721(path_table_l + path_table_size, 1);
      set_722(path_table_m + path_table_size, 1);
      path_table_size += 2;

      for(j=0; j<len; j++)
      {
         path_table_l[path_table_size] = root_dir_entry[i+33+j];
         path_table_m[path_table_size] = root_dir_entry[i+33+j];
         path_table_size++;
      }

      if(path_table_size & 1) path_table_size++;
   }
}
*/

/* From mkisofs: */

#define ISODCL(from, to) (to - from + 1)

static struct iso_primary_descriptor {
        char type                       [ISODCL (  1,   1)]; /* 711 */
        char id                         [ISODCL (  2,   6)];
        char version                    [ISODCL (  7,   7)]; /* 711 */
        char unused1                    [ISODCL (  8,   8)];
        char system_id                  [ISODCL (  9,  40)]; /* achars */
        char volume_id                  [ISODCL ( 41,  72)]; /* dchars */
        char unused2                    [ISODCL ( 73,  80)];
        char volume_space_size          [ISODCL ( 81,  88)]; /* 733 */
        char escape_sequences           [ISODCL ( 89, 120)];
        char volume_set_size            [ISODCL (121, 124)]; /* 723 */
        char volume_sequence_number     [ISODCL (125, 128)]; /* 723 */
        char logical_block_size         [ISODCL (129, 132)]; /* 723 */
        char path_table_size            [ISODCL (133, 140)]; /* 733 */
        char type_l_path_table          [ISODCL (141, 144)]; /* 731 */
        char opt_type_l_path_table      [ISODCL (145, 148)]; /* 731 */
        char type_m_path_table          [ISODCL (149, 152)]; /* 732 */
        char opt_type_m_path_table      [ISODCL (153, 156)]; /* 732 */
        char root_directory_record      [ISODCL (157, 190)]; /* 9.1 */
        char volume_set_id              [ISODCL (191, 318)]; /* dchars */
        char publisher_id               [ISODCL (319, 446)]; /* achars */
        char preparer_id                [ISODCL (447, 574)]; /* achars */
        char application_id             [ISODCL (575, 702)]; /* achars */
        char copyright_file_id          [ISODCL (703, 739)]; /* 7.5 dchars */
        char abstract_file_id           [ISODCL (740, 776)]; /* 7.5 dchars */
        char bibliographic_file_id      [ISODCL (777, 813)]; /* 7.5 dchars */
        char creation_date              [ISODCL (814, 830)]; /* 8.4.26.1 */
        char modification_date          [ISODCL (831, 847)]; /* 8.4.26.1 */
        char expiration_date            [ISODCL (848, 864)]; /* 8.4.26.1 */
        char effective_date             [ISODCL (865, 881)]; /* 8.4.26.1 */
        char file_structure_version     [ISODCL (882, 882)]; /* 711 */
        char unused4                    [ISODCL (883, 883)];
        char application_data           [ISODCL (884, 1395)];
        char unused5                    [ISODCL (1396, 2048)];
} ipd;

static struct iso_directory_record {
        unsigned char length            [ISODCL (1, 1)];   /* 711 */
        char ext_attr_length            [ISODCL (2, 2)];   /* 711 */
        char extent                     [ISODCL (3, 10)];  /* 733 */
        char size                       [ISODCL (11, 18)]; /* 733 */
        char date                       [ISODCL (19, 25)]; /* 7 by 711 */
        char flags                      [ISODCL (26, 26)];
        char file_unit_size             [ISODCL (27, 27)]; /* 711 */
        char interleave                 [ISODCL (28, 28)]; /* 711 */
        char volume_sequence_number     [ISODCL (29, 32)]; /* 723 */
        unsigned char name_len          [ISODCL (33, 33)]; /* 711 */
        char name                       [34]; /* Not really, but we need something here */
} idr;



static void add_dirent(char *dir, int *dirlen, int dirsize,
           char *name, int namelen, int extent, int size, int flags, int xa)
{
   int reclen;
   unsigned char xa_attr[2];

   reclen = 33 + namelen;
   if(reclen & 1) reclen++;

   if(xa >= 0) reclen += 14; // add XA signature

   if (reclen > (ISO_DIR_SIZE-(*dirlen%ISO_DIR_SIZE)))  // avoid directory record crossing block boundaries
      *dirlen += (ISO_DIR_SIZE-(*dirlen%ISO_DIR_SIZE));

   if (dir)
   {
      if ((*dirlen) + reclen > dirsize)
      {
         fprintf(stderr,"Fatal Error: add_dirent - max dirsize exceeded\n");
         exit(1);
      }

      memset(dir+(*dirlen), 0, reclen);
   }

   memset(&idr, 0 , sizeof(idr));

   idr.length[0] = reclen;
   idr.ext_attr_length[0] = 0;
   set_733(idr.extent,extent);
   set_733(idr.size,size);

   idr.date[0] = t->tm_year;
   idr.date[1] = t->tm_mon+1;
   idr.date[2] = t->tm_mday;
   idr.date[3] = t->tm_hour;
   idr.date[4] = t->tm_min;
   idr.date[5] = t->tm_sec;
   idr.date[6] = 0; /* This is in units of 15 minutes from GMT */

   idr.flags[0] = flags;
   idr.file_unit_size[0] = 0;
   idr.interleave[0] = 0;

   set_723(idr.volume_sequence_number,1);

   idr.name_len[0] = namelen;

   if (dir)
   {
      memcpy(dir+(*dirlen), &idr, 33);
      memcpy(dir+(*dirlen)+33, name, namelen);
   }

   /* XA signature */

   if (xa >= 0)
   {
      if (flags == 2) {
         xa_attr[0] = 0x8d;  // directory
         xa_attr[1] = 0x55;
      } else {
         if (xa == 0) {
            xa_attr[0] = 0x0d;  // form1 file
            xa_attr[1] = 0x55;
         } else {
            xa_attr[0] = 0x15;  // form2 (VCD) file
            xa_attr[0] = 0x35;  // form2 (PS1) file
            xa_attr[1] = 0x55;
         }
      }
      
      if (dir)
      {
         dir[*dirlen+reclen-10] = xa_attr[0];
         dir[*dirlen+reclen- 9] = xa_attr[1];
         dir[*dirlen+reclen- 8] = 'X';
         dir[*dirlen+reclen- 7] = 'A';
         dir[*dirlen+reclen- 6] = xa;
      }
   }

   (*dirlen) += reclen;
}

extern int last_extent;
extern int starting_lba;
static void make_ipd(dir_s **dir_entry)
{
   char iso_time[17];
   int len;
   char name[1];

   memset(&ipd,0,sizeof(ipd));

   ipd.type[0] = 1;
   set_str(ipd.id,"CD001",5);
   ipd.version[0] = 1;

   set_str(ipd.system_id,CD_SYSTEM_ID,32);
   set_str(ipd.volume_id,CD_VOLUME_ID,32);

   set_733(ipd.volume_space_size,last_extent);
   /* we leave ipd.escape_sequences = 0 */

   set_723(ipd.volume_set_size,1);
   set_723(ipd.volume_sequence_number,1);
   set_723(ipd.logical_block_size,2048);

   set_733(ipd.path_table_size,path_table_size);
   set_731(ipd.type_l_path_table,PATH_TABLE_L_EXTENT);
   set_731(ipd.opt_type_l_path_table,0);
   set_732(ipd.type_m_path_table,PATH_TABLE_M_EXTENT);
   set_732(ipd.opt_type_m_path_table,0);

   len = 0;
   name[0] = 0;

	 if( starting_lba > 0 )
		 ROOT_DIR_EXTENT = starting_lba;

	 add_dirent(ipd.root_directory_record, &len, 34,
		          name, 1, ROOT_DIR_EXTENT, LEN2BLOCKS(dir_entry[0]->len)*ISO_DIR_SIZE, 2, -1);
   if(len!=34)
   {
      fprintf(stderr,"Internal error in make_ipd\n");
      exit(1);
   }

   set_str(ipd.volume_set_id , CD_VOLUME_SET_ID , 128);
   set_str(ipd.publisher_id  , CD_PUBLISHER_ID  , 128);
   set_str(ipd.preparer_id   , CD_PREPARER_ID   , 128);
   set_str(ipd.application_id, CD_APPLICATION_ID, 128);

   set_str(ipd.copyright_file_id    , " ", 37);
   set_str(ipd.abstract_file_id     , " ", 37);
   set_str(ipd.bibliographic_file_id, " ", 37);

   sprintf(iso_time, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d00",
              1900+(t->tm_year), t->tm_mon+1, t->tm_mday,
              t->tm_hour, t->tm_min, t->tm_sec);
   iso_time[16] = 0; /* This is in binary! Units of 15 minutes from GMT */

   memcpy(ipd.creation_date,    iso_time,17);
   memcpy(ipd.modification_date,iso_time,17);
   memcpy(ipd.expiration_date,  "0000000000000000\0",17);
   memcpy(ipd.effective_date,   iso_time,17);

   ipd.file_structure_version[0] = 1;

   /*  The string CD-XA001 in this exact position indicates  */
   /*  that the disk contains XA sectors.                    */
   memcpy(ipd.application_data+141, "CD-XA001", 8);
}

static void init_iso_dir(char *dir, int *dirlen, int self_dir_len, int parent_dir_len, int self, int parent)
{
   char name[1];

   if (dir) memset(dir,0,self_dir_len);
   *dirlen = 0;

   /* add . and .. entries */

   name[0] = 0;
   add_dirent(dir, dirlen, self_dir_len, name, 1, self,   self_dir_len, 2, 0);

   name[0] = 1;
   add_dirent(dir, dirlen, self_dir_len, name, 1, parent, parent_dir_len, 2, 0);
}

void to_dchar(char *dest, char *src, int max)
{
int j;

   for (j = 0; j < (strlen(src) <= max ? strlen(src) : max); j++)
       dest[j] = (isalnum(src[j]) ? toupper(src[j]) : '_'); // d-char (ISO-9660)
   dest[j] = 0;
}

void convert_name(char *dstname, char *srcname, int type, opts_s *opts)
{
char *name, *ext;
char FORM1_EXT[MAX_PATH] = { 0, };
int j;

         if (!opts->ascii && type != DIR)
            to_dchar(FORM2_EXT, FORM2_EXT, 30);

         for (j = strlen(srcname); j > 0; j--)
         {
             if (srcname[j-1] == DIR_SLASH || srcname[j-1] == ':')
                break;
         }
         name = &srcname[j];
/*
         name = (char *) strrchr(srcname, DIR_SLASH);

         if (name == NULL)
            name = srcname;
         else
            name = &name[1];  // skip directory slash
*/
         strcpy(dstname, name);

         if (type != DIR)
         {
            ext = (char *) strrchr(dstname, '.');

            if (ext != NULL)
            {
               if (type == FORM1)
               {
                  strcpy(FORM1_EXT,&ext[1]);
                  if (!opts->ascii) to_dchar(FORM1_EXT, FORM1_EXT, 30);
                  ext[0] = 0; // trim the file extension, if any
               }
               else if( type == FORM2 ) // FORM2
                  if (!opts->form2_ext_to_name) ext[0] = 0; // trim the file extension, if any
            }
         }

         if (!opts->long_file_names)
         {
            if (strlen(dstname) > 8) dstname[8] = 0;

            if (type != DIR)
            {
               if (strlen(FORM1_EXT) > 3) FORM1_EXT[3] = 0;
               if (strlen(FORM2_EXT) > 3) FORM2_EXT[3] = 0;
            }
         }

         if (!opts->ascii)
            if (type != DIR)
               to_dchar(dstname, dstname, 30-(strlen(type == FORM1 ? FORM1_EXT : FORM2_EXT)));
            else
               to_dchar(dstname, dstname, 31);  // extensions not allowed in dirs on ISO-9660

         if (type != DIR)
         {
						if( type != FORM2_PS1 && type != FORM2_CDDA )
							strcat(dstname, "."); // separator is mandatory

            if (type == FORM2)
               strcat(dstname, FORM2_EXT);
            else // FORM1
               if (ext != NULL) strcat(dstname, FORM1_EXT);

            strcat(dstname, ";1"); // file version number
         }

#ifdef _WIN32
         if (opts->ascii) CharToOem(dstname, dstname); // use DOS character set for international file names
#endif
}

int get_parent(entry_s **entry, int current)
{
int n, number;

    number = 0;

    if (entry[current]->parent == 0) return 0; // root

    for (n=0;n<current;n++)
    {
        if (entry[n]->type == DIR)
        {
           number++;
           if (entry[current]->parent == entry[n]->number) // dir ID and parent ID are equal
              return number;
        }
    }

return -1; // unknown parent
}

extern int starting_lba;
int mk_vcd_iso_fs(entry_s **entry, int num_entries, dir_s **dir_entry, int num_dirs, opts_s *opts, int pass)
{
   int i, n, len;
   char filename[MAX_PATH];
   char dirname[MAX_PATH];
   int self_dir_len, parent_dir_len;
   int current_dir, cur_dir_extent;
   int cur_dir_len[MAX_DIRS] = { 0, };
   int parent_dir;
   time_t T;

   /* some initializations */

	 if( starting_lba )
		 ROOT_DIR_EXTENT = starting_lba;

   time(&T);
   t = gmtime(&T);

   /* Allocate memory for directory records on PASS_3 */

   if (pass == PASS_3)
      for (i = 0; i < num_dirs; i++)
          dir_entry[i]->record = (char *) malloc(LEN2BLOCKS(dir_entry[i]->len)*ISO_DIR_SIZE);

   dirname[0] = 0;
   current_dir = 0;
   parent_dir = 0;
   cur_dir_extent = ROOT_DIR_EXTENT;

   if (pass == PASS_1)
      self_dir_len = 0;
   else
      self_dir_len = LEN2BLOCKS(dir_entry[0]->len)*ISO_DIR_SIZE;

   init_iso_dir(dir_entry[current_dir]->record, &cur_dir_len[current_dir], self_dir_len, self_dir_len, cur_dir_extent, cur_dir_extent);

   dir_entry[0]->extent = ROOT_DIR_EXTENT;
   dir_entry[0]->parent = ROOT_DIR_EXTENT;
   dir_entry[0]->parent_num = 1;
   dir_entry[0]->name[0] = 0;

   for(n=0;n<num_entries;n++)
   {
		 if( n==1 && starting_lba>0 )
		 {
			 //cur_dir_extent = starting_lba;
		 }


      if (entry[n]->type == DIR) // new dir
      {
         current_dir++;

         parent_dir = get_parent(entry, n);

         if (parent_dir == -1)
         {
         fprintf(stderr,"Error: unknown directory specified as parent: %s", entry[n]->name);
         exit(1);
         }

         if (pass == PASS_1) {
            self_dir_len = 0;
            parent_dir_len = 0;
         } else {
            self_dir_len = LEN2BLOCKS(dir_entry[current_dir]->len)*ISO_DIR_SIZE;
            parent_dir_len = LEN2BLOCKS(dir_entry[parent_dir]->len)*ISO_DIR_SIZE;
         }

         convert_name(dirname, entry[n]->name, entry[n]->type, opts);
         strcpy(dir_entry[current_dir]->name, dirname);
         if (pass == PASS_3) printf("%s\\\n",dirname);

         if (pass != PASS_1) cur_dir_extent += (LEN2BLOCKS(dir_entry[current_dir-1]->len));

         dir_entry[current_dir]->extent = cur_dir_extent;
         dir_entry[current_dir]->parent = dir_entry[parent_dir]->extent;
         dir_entry[current_dir]->parent_num = parent_dir+1;

         /* Initialize directory */

         init_iso_dir(dir_entry[current_dir]->record, &cur_dir_len[current_dir], self_dir_len, parent_dir_len, cur_dir_extent, dir_entry[current_dir]->parent);

         /* Add this directory to parent directory */

         add_dirent(dir_entry[parent_dir]->record, &cur_dir_len[parent_dir], parent_dir_len,
                   dirname, strlen(dirname), cur_dir_extent, self_dir_len, 2, 0);
      }
      else if (entry[n]->type != NOTFILE) // files
      {
         convert_name(filename, entry[n]->name, entry[n]->type, opts);
         if (pass == PASS_3) printf("%s\\%s\n",dirname,filename);

         if (entry[n]->type == FORM2)
            len = entry[n]->size*2048; // dummy size for FORM2 files
         else if (entry[n]->type == FORM2_PS1)
            len = entry[n]->size*2048; // dummy size for FORM2 files
         else if (entry[n]->type == FORM2_CDDA)
            len = entry[n]->size*2048; // dummy size for FORM2 files
         else
            len = entry[n]->length;

         add_dirent(dir_entry[current_dir]->record, &cur_dir_len[current_dir], self_dir_len,
                   filename, strlen(filename), entry[n]->extent, len, 0, entry[n]->number);
      }
   }

   cur_dir_extent += (LEN2BLOCKS(cur_dir_len[current_dir]));

   if (pass == PASS_1)
      for (i = 0; i < current_dir+1; i++)
          dir_entry[i]->len = cur_dir_len[i]; // fill dir_entry[]->len with directory sizes

   if (pass == PASS_3) // do not output data until PASS_3
   {
      /* Output boot record */

      for(i=0;i<PVD_EXTENT;i++) output_form1(i,zero2048);

      /* Output PVD, EVD & Path Tables */

      make_path_tables(dir_entry, num_dirs);
      make_ipd(dir_entry);

      memset(buff2048, 0, 2048);

      buff2048[0] = 0xff;
      buff2048[1] = 'C';
      buff2048[2] = 'D';
      buff2048[3] = '0';
      buff2048[4] = '0';
      buff2048[5] = '1';
      buff2048[6] = 0x01;

      output_form1(PVD_EXTENT,(char *) &ipd);
      output_form1(EVD_EXTENT, buff2048);
      output_form1(PATH_TABLE_L_EXTENT,path_table_l);
      output_form1(PATH_TABLE_L_EXTENT+1,zero2048);
      output_form1(PATH_TABLE_M_EXTENT,path_table_m);
      output_form1(PATH_TABLE_M_EXTENT+1,zero2048);

      /* Output directory entries */

      for(i = 0; i < num_dirs; i++)
         for(n = 0; n < (LEN2BLOCKS(dir_entry[i]->len)); n++)
            output_form1(dir_entry[i]->extent+n, dir_entry[i]->record+(n*ISO_DIR_SIZE));

      /* Fill the rest of the ISO FS with empty sectors */

      for(i=cur_dir_extent;i<START_FILE_EXTENT;i++)
         output_form1(i,zero2048);

   } // end: if (pass == PASS_3)

   return cur_dir_extent;
}







#include "buffer.h"
#include "raw96.h"

extern struct buffer_s bin_file_buffer;

unsigned char read_buffer1[ 2352*2048 ];
unsigned int read_size, read_ptr;

FILE *fd, *cd_log;

int debug;
extern int log_lba_in_log;

void copy_ps1_file( char *dir_name )
{
	FILE *fp;
	int lcv, lcv2;
	int mode2;

	char path_name[512];
	int old_ftell;


	old_ftell = ftell(fd);
	printf( "%s\n", idr.name );


	strcpy( path_name, "out_cd\\" );
	strcat( path_name, dir_name );
	strcat( path_name, "\\" );
	strcat( path_name, idr.name );

	fp = fopen( path_name, "wb" );
	if( !fp ) return;


	// check form1/form2/cdda
	lcv = get_731(idr.size);
	fseek( fd, 2352 * get_731(idr.extent), SEEK_SET );

	lcv2 = 30;
	mode2 = 0;
	while( lcv>0 && lcv2>0)
	{
		// check cdda
		fread( &buff2352, 1, (12+4)+2, fd );
		if( buff2352[0] != 0x00 ||
				buff2352[1] != 0xff ||
				buff2352[2] != 0xff ||
				buff2352[3] != 0xff ||
				buff2352[4] != 0xff ||
				buff2352[5] != 0xff ||
				buff2352[6] != 0xff ||
				buff2352[7] != 0xff )
		{
			mode2 = 2;
			break;
		}


		// check header (mode2)
		if( fgetc(fd) & (1<<5) )
		{
			mode2 = 1;
			break;
		}

		fseek( fd, (1+4)+2048+280, SEEK_CUR );
		lcv -= 2048;

		lcv2--;
	}


	// skip to data
	fseek( fd, 2352 * get_731(idr.extent), SEEK_SET );
	lcv = get_731(idr.size);


	// buffer input file
	read_size = (( get_731(idr.size)+2047 ) / 2048) * 2352;
	fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
	read_ptr = 0;



	if (!BufInit(fp, WRITE_BUF_SIZE, &bin_file_buffer))
	{
		fprintf(stderr,"Not enough free memory for buffers!");
		return;
	}


	// form1 (2048)
	if( mode2 == 0 )
	{
		strcpy( path_name, dir_name );
		if( dir_name[0] )
		{
			strcat( path_name, "\\" );
		}
		strcat( path_name, idr.name );

		if( log_lba_in_log )
		{
			fprintf( cd_log, "-set_lba\n" );
			fprintf( cd_log, "%X\n", get_731(idr.extent) );
		}

		fprintf( cd_log, "-f\n" );
		fprintf( cd_log, "%s\n", path_name );

		if( log_lba_in_log )
		{
			fprintf( cd_log, "\n" );
		}


		while( lcv>0 )
		{
			// reset ptr
			if( read_ptr >= 2352*2048 )
			{
				read_size -= 2352*2048;

				fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
				read_ptr = 0;
			}


			// read mode2-2048
			read_ptr += (12+4+4+4);
			BufWrite( read_buffer1 + read_ptr, (lcv>2048)?2048:lcv, &bin_file_buffer);
			read_ptr += 2048;
			read_ptr += 280;

			lcv -= 2048;
		}
	}

	// form2 (2336)
	else if( mode2 == 1 )
	{
		strcpy( path_name, dir_name );
		if( dir_name[0] )
		{
			strcat( path_name, "\\" );
		}
		strcat( path_name, idr.name );

		if( log_lba_in_log )
		{
			fprintf( cd_log, "-set_lba\n" );
			fprintf( cd_log, "%X\n", get_731(idr.extent) );
		}

		fprintf( cd_log, "-ps1\n" );
		fprintf( cd_log, "%s\n", path_name );

		if( log_lba_in_log )
		{
			fprintf( cd_log, "\n" );
		}


		while( lcv>0 )
		{
			// reset ptr
			if( read_ptr >= 2352*2048 )
			{
				read_size -= 2352*2048;

				fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
				read_ptr = 0;
			}


			// read mode2-2336
			read_ptr += (12+4);
			BufWrite( read_buffer1 + read_ptr, 2336, &bin_file_buffer);
			read_ptr += 2336;

			lcv -= 2048;
		}
	}

	// form2-cdda (2352)
	else if( mode2 == 2 )
	{
		strcpy( path_name, dir_name );
		if( dir_name[0] )
		{
			strcat( path_name, "\\" );
		}
		strcat( path_name, idr.name );

		if( log_lba_in_log )
		{
			fprintf( cd_log, "-set_lba\n" );
			fprintf( cd_log, "%X\n", get_731(idr.extent) );
		}

		fprintf( cd_log, "-cdda\n" );
		fprintf( cd_log, "%s\n", path_name );

		if( log_lba_in_log )
		{
			fprintf( cd_log, "\n" );
		}



		while( lcv>0 )
		{
			// reset ptr
			if( read_ptr >= 2352*2048 )
			{
				read_size -= 2352*2048;

				fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
				read_ptr = 0;
			}


			// read mode2-2352
			BufWrite( read_buffer1 + read_ptr, 2352, &bin_file_buffer);
			read_ptr += 2352;

			lcv -= 2048;
		}
	}

  BufWriteFlush(&bin_file_buffer);
	BufFree(&bin_file_buffer);

	lcv = ftell(fd);
	fclose( fp );

	fseek( fd, old_ftell, SEEK_SET );
}


void extract_ps1_dir( char *dir_name, int LBA )
{
	char path_name[512];
	int old_ftell;

	int block_count;
	int block_size;
	int stop;

	if( dir_name[0] )
	{
		strcpy( path_name, "out_cd\\" );
		strcat( path_name, dir_name );

		mkdir( path_name );

		fprintf( cd_log, "\n\n-dir\n" );
		fprintf( cd_log, "%s\n", dir_name );

		printf( "%s\n", dir_name );
	}

	// skip header
	fseek( fd, 2352 * LBA, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );

	// read files (no directories)
	block_count = 0;
	while(1)
	{
		int first_time;

		first_time = 0;
		block_size = 0;
		stop = 0;

		block_count++;
		while(1)
		{
			memset( &idr, 0, sizeof(idr) );
			if( block_size > 2048-33 ) break;

			idr.length[0] = fgetc(fd);
			fread( idr.ext_attr_length, 1, idr.length[0]-1, fd );
			if( (ftell(fd)%2) == 1 ) fgetc(fd);

			block_size += idr.length[0];


			// check for directory spanning
			if( block_count > 1 )
			{
				int lcv;

				// do not enter subdirectories
				for( lcv=0; lcv < dir_count; lcv++ )
				{
					if( LBA+block_count-1 == dir_lba[lcv] )
					{
						stop = 1;
						break;
					}
				}
				if( stop ) break;


				// check for valid tag
				lcv = idr.length[0]-1;
				if( lcv>0 )
				{
					while( lcv>1 && idr.length[lcv]<0x20 ) lcv--;
					if( idr.length[ lcv-1 ] != 'X' ||
							idr.length[ lcv-0 ] != 'A' )
						stop = 1;
				}
				else
					stop = 1;


				if( stop ) break;
			}

			first_time = 1;


			// check DUMMY
			if( idr.length[0] == 0 ) break;
			if( idr.name[0] < ' ' ) continue;

			// verify real FILE
			if( idr.name[ idr.name_len[0]-2 ] == ';' &&
					idr.name[ idr.name_len[0]-1 ] == '1' )
			{
				idr.name[ idr.name_len[0]-2 ] = 0;

				copy_ps1_file( dir_name );
			}
		}

		// allow for boundary spanning
		if( stop && first_time==0 ) break;

		fseek( fd, 2352 * (LBA + block_count), SEEK_SET );
		fseek( fd, 24, SEEK_CUR );
	}


	// skip header
	fseek( fd, 2352 * LBA, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );

	// read ONLY dirs
	block_count = 0;
	while(1)
	{
		int first_time;

		first_time = 0;
		block_size = 0;
		stop = 0;

		block_count++;
		while(1)
		{
			memset( &idr, 0, sizeof(idr) );
			if( block_size > 2048-33 ) break;

			idr.length[0] = fgetc(fd);
			fread( idr.ext_attr_length, 1, idr.length[0]-1, fd );
			if( (ftell(fd)%2) == 1 ) fgetc(fd);

			block_size += idr.length[0];


			// check for directory spanning
			if( block_count > 1 )
			{
				int lcv;

				// do not enter subdirectories
				for( lcv=0; lcv < dir_count; lcv++ )
				{
					if( LBA+block_count-1 == dir_lba[lcv] )
					{
						stop = 1;
						break;
					}
				}
				if( stop ) break;


				// check for valid tag
				lcv = idr.length[0]-1;
				if( lcv>0 )
				{
					while( lcv>1 && idr.length[lcv]<0x20 ) lcv--;
					if( idr.length[ lcv-1 ] != 'X' ||
							idr.length[ lcv-0 ] != 'A' )
						stop = 1;
				}
				else
					stop = 1;


				if( stop ) break;
			}

			first_time = 1;


			// check DUMMY
			if( idr.length[0] == 0 ) break;
			if( idr.name[0] < ' ' ) continue;

			// verify real FILE
			if( idr.name[ idr.name_len[0]-2 ] == ';' &&
					idr.name[ idr.name_len[0]-1 ] == '1' )
				continue;


			// next directory
			strcpy( path_name, dir_name );
			if( dir_name[0] )
			{
				strcat( path_name, "\\" );
			}
			strcat( path_name, idr.name );

			old_ftell = ftell(fd);
			extract_ps1_dir( path_name, get_731(idr.extent) );
			fseek( fd, old_ftell, SEEK_SET );
		}

		// allow for boundary spanning
		if( stop && first_time==0 ) break;

		fseek( fd, 2352 * (LBA + block_count), SEEK_SET );
		fseek( fd, 24, SEEK_CUR );
	}
}

void extract_ps1( char *iso_name )
{
	mkdir( "out_cd" );

	cd_log = fopen( "out_cd\\___cd.txt", "w" );
	if( !cd_log ) return;

	fd = fopen( iso_name, "rb" );
	if( !fd ) return;


	// skip header and read file info
	fseek( fd, 2352 * 16, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );
	fread( &ipd, 1, 1024, fd );

	// read in directory locations
	fseek( fd, 2352 * PATH_TABLE_L_EXTENT, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );
	while(1)
	{
		int name_len, extent, dummy;

		name_len = fgetc(fd);
		if( name_len == 0 ) break;

		dummy = fgetc(fd);
		fread( &dir_lba[dir_count++], 1, 4, fd );

		dummy = fgetc(fd);
		dummy = fgetc(fd);

		fread( &buff2352, 1, name_len, fd );
		if( name_len%2 ) fgetc(fd);
	}

	fprintf( cd_log, "-s\n" );
	fprintf( cd_log, "-v\n" );
	fprintf( cd_log, "%s\n", ipd.volume_id );
	fprintf( cd_log, "\n\n" );


	extract_ps1_dir( "", get_721( ipd.root_directory_record+2 ) );


	fclose( fd );
	fclose( cd_log );
}



void list_ps1_dir( char *dir_name, int LBA )
{
	char path_name[512];
	int old_ftell;

	int block_count;
	int stop;
	int block_size;

	if( dir_name[0] )
	{
		fprintf( cd_log, "%s\n", dir_name );
	}

	// skip header
	fseek( fd, 2352 * LBA, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );

	//if( strcmp(dir_name,"MOVIE")==0 )
		//LBA+=0;

	// read files (no directories)
	block_count = 0;
	while(1)
	{
		int first_time;

		first_time = 0;
		block_size = 0;
		stop = 0;

		block_count++;
		while(1)
		{
			memset( &idr, 0, sizeof(idr) );
			if( block_size > 2048-33 ) break;

			idr.length[0] = fgetc(fd);
			fread( idr.ext_attr_length, 1, idr.length[0]-1, fd );
			if( (ftell(fd)%2) == 1 ) fgetc(fd);

			block_size += idr.length[0];


			// check for directory spanning
			if( block_count > 1 )
			{
				int lcv;

				// do not enter subdirectories
				for( lcv=0; lcv < dir_count; lcv++ )
				{
					if( LBA+block_count-1 == dir_lba[lcv] )
					{
						stop = 1;
						break;
					}
				}
				if( stop ) break;


				// check for valid tag
				lcv = idr.length[0]-1;
				if( lcv>0 )
				{
					while( lcv>1 && idr.length[lcv]<0x20 ) lcv--;
					if( idr.length[ lcv-1 ] != 'X' ||
							idr.length[ lcv-0 ] != 'A' )
						stop = 1;
				}
				else
					stop = 1;


				if( stop ) break;
			}

			first_time = 1;


			// check DUMMY
			if( idr.length[0] == 0 ) break;
			if( idr.name[0] < ' ' ) continue;

			// verify real FILE
			if( idr.name[ idr.name_len[0]-2 ] == ';' &&
					idr.name[ idr.name_len[0]-1 ] == '1' )
			{
				unsigned int start,end;

				idr.name[ idr.name_len[0]-2 ] = 0;

				start = get_731(idr.extent);
				end = start + LEN2BLOCKS(get_731(idr.size)) - 1;

				// list LBA blocks
				fprintf( cd_log, "%5X-%5X [%s] [%d:%d.%d - %d:%d.%d]\n",
					start, end,
					idr.name,
					LBA2MIN(start), LBA2SEC(start), LBA2FRA(start),
					LBA2MIN(end), LBA2SEC(end), LBA2FRA(end) );
			}
		}

		// allow for boundary spanning
		if( stop && first_time==0 ) break;

		fseek( fd, 2352 * (LBA + block_count), SEEK_SET );
		fseek( fd, 24, SEEK_CUR );
	}


	// skip header
	fseek( fd, 2352 * LBA, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );

	//if( strcmp(dir_name,"MAGIC")==0 )
		//LBA+=0;

	// read ONLY dirs
	block_count = 0;
	while(1)
	{
		int first_time;

		first_time = 0;
		block_size = 0;
		stop = 0;

		block_count++;
		while(1)
		{
			memset( &idr, 0, sizeof(idr) );
			if( block_size > 2048-33 ) break;

			idr.length[0] = fgetc(fd);
			fread( idr.ext_attr_length, 1, idr.length[0]-1, fd );
			if( (ftell(fd)%2) == 1 ) fgetc(fd);

			block_size += idr.length[0];


			// check for directory spanning
			if( block_count > 1 )
			{
				int lcv;

				// do not enter subdirectories
				for( lcv=0; lcv < dir_count; lcv++ )
				{
					if( LBA+block_count-1 == dir_lba[lcv] )
					{
						stop = 1;
						break;
					}
				}
				if( stop ) break;


				// check for valid tag
				lcv = idr.length[0]-1;
				if( lcv>0 )
				{
					while( lcv>1 && idr.length[lcv]<0x20 ) lcv--;
					if( idr.length[ lcv-1 ] != 'X' ||
							idr.length[ lcv-0 ] != 'A' )
						stop = 1;
				}
				else
					stop = 1;


				if( stop ) break;
			}

			first_time = 1;


			// check DUMMY
			if( idr.length[0] == 0 ) break;
			if( idr.name[0] < ' ' ) continue;

			// verify real FILE
			if( idr.name[ idr.name_len[0]-2 ] == ';' &&
					idr.name[ idr.name_len[0]-1 ] == '1' )
				continue;


			// next directory
			strcpy( path_name, dir_name );
			if( dir_name[0] )
			{
				strcat( path_name, "\\" );
			}
			strcat( path_name, idr.name );

			old_ftell = ftell(fd);
			list_ps1_dir( path_name, get_731(idr.extent) );
			fseek( fd, old_ftell, SEEK_SET );
		}

		// allow for boundary spanning
		if( stop && first_time==0 ) break;

		fseek( fd, 2352 * (LBA + block_count), SEEK_SET );
		fseek( fd, 24, SEEK_CUR );
	}
}

void list_ps1( char *iso_name )
{
	cd_log = fopen( "___cd_lba.txt", "w" );
	if( !cd_log ) return;

	fd = fopen( iso_name, "rb" );
	if( !fd ) return;


	// skip header and read file info
	fseek( fd, 2352 * 16, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );
	fread( &ipd, 1, 1024, fd );


	// read in directory locations
	fseek( fd, 2352 * PATH_TABLE_L_EXTENT, SEEK_SET );
	fseek( fd, 24, SEEK_CUR );
	while(1)
	{
		int name_len, extent, dummy;

		name_len = fgetc(fd);
		if( name_len == 0 ) break;

		dummy = fgetc(fd);
		fread( &dir_lba[dir_count++], 1, 4, fd );

		dummy = fgetc(fd);
		dummy = fgetc(fd);

		fread( &buff2352, 1, name_len, fd );
		if( name_len%2 ) fgetc(fd);
	}


	list_ps1_dir( "", dir_lba[0] );


	fclose( fd );
	fclose( cd_log );
}

void read_cdda( char *iso_name, int start, int size, char *out_name )
{
	FILE *fp;
	int lcv;
	int mode2;

	char path_name[512];


	fd = fopen( iso_name, "rb" );
	if( !fd ) return;


	strcpy( path_name, out_name );

	fp = fopen( path_name, "wb" );
	if( !fp ) return;


	// skip to data
	fseek( fd, 2352 * start, SEEK_SET );
	lcv = size;


	// buffer input file
	read_size = ((size+2047) / 2048) * 2352;
	fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
	read_ptr = 0;



	if (!BufInit(fp, WRITE_BUF_SIZE, &bin_file_buffer))
	{
		fprintf(stderr,"Not enough free memory for buffers!");
		return;
	}


	// form2 (2352)
	while( lcv>0 )
	{
		// reset ptr
		if( read_ptr >= 2352*2048 )
		{
			read_size -= 2352*2048;

			fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
			read_ptr = 0;
		}


		// read mode2-2352
		BufWrite( read_buffer1 + read_ptr, 2352, &bin_file_buffer);
		read_ptr += 2352;

		lcv -= 2048;
	}

  BufWriteFlush(&bin_file_buffer);
	BufFree(&bin_file_buffer);

	fclose( fp );
	fclose( fd );
}

void read_form2( char *iso_name, int start, int size, char *out_name )
{
	FILE *fp;
	int lcv;
	int mode2;

	char path_name[512];


	fd = fopen( iso_name, "rb" );
	if( !fd ) return;


	strcpy( path_name, out_name );

	fp = fopen( path_name, "wb" );
	if( !fp ) return;


	// skip to data
	fseek( fd, 2352 * start, SEEK_SET );
	lcv = size;


	// buffer input file
	read_size = ((size+2047) / 2048) * 2352;
	fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
	read_ptr = 0;



	if (!BufInit(fp, WRITE_BUF_SIZE, &bin_file_buffer))
	{
		fprintf(stderr,"Not enough free memory for buffers!");
		return;
	}


	// form2 (2336)
	while( lcv>0 )
	{
		// reset ptr
		if( read_ptr >= 2352*2048 )
		{
			read_size -= 2352*2048;

			fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
			read_ptr = 0;
		}


		// read mode2-2336
		read_ptr += (12+4);
		BufWrite( read_buffer1 + read_ptr, 2336, &bin_file_buffer);
		read_ptr += 2336;

		lcv -= 2048;
	}

  BufWriteFlush(&bin_file_buffer);
	BufFree(&bin_file_buffer);

	fclose( fp );
	fclose( fd );
}

void read_form1( char *iso_name, int start, int size, char *out_name )
{
	FILE *fp;
	int lcv;
	int mode2;

	char path_name[512];


	fd = fopen( iso_name, "rb" );
	if( !fd ) return;


	strcpy( path_name, out_name );

	fp = fopen( path_name, "wb" );
	if( !fp ) return;


	// skip to data
	fseek( fd, 2352 * start, SEEK_SET );
	lcv = size;


	// buffer input file
	read_size = ((size+2047) / 2048) * 2352;
	fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
	read_ptr = 0;



	if (!BufInit(fp, WRITE_BUF_SIZE, &bin_file_buffer))
	{
		fprintf(stderr,"Not enough free memory for buffers!");
		return;
	}


	// form1 (2048)
	while( lcv>0 )
	{
		// reset ptr
		if( read_ptr >= 2352*2048 )
		{
			read_size -= 2352*2048;

			fread( read_buffer1, read_size>2352*2048 ? 2352*2048 : read_size, 1, fd );
			read_ptr = 0;
		}


		// read mode2-2048
		read_ptr += (12+4+4+4);
		BufWrite( read_buffer1 + read_ptr, (lcv>2048)?2048:lcv, &bin_file_buffer);
		read_ptr += 2048;
		read_ptr += 280;

		lcv -= 2048;
	}

  BufWriteFlush(&bin_file_buffer);
	BufFree(&bin_file_buffer);

	fclose( fp );
	fclose( fd );
}
