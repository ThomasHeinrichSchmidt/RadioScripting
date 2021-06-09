/* create GET query line 
 *    GET  /ws/2/recording?query=Winter+Wonderland%20AND%20artist:Aretha+Franklin&type:song&limit=1  HTTP/1.1
 * from playinfo.xml 
 *    <logo_img>http://127.0.0.1:8080/playlogo.jpg</logo_img><stream_format>MP3 /128 Kbps</stream_format><station_info> 1000 Christmashits/ Das Weihnachtsradio vn 1000Radiohits.de.</station_info><song>Winter Wonderland</song><artist>Aretha Franklin</artist>
 *
 * artist := <artist>Aretha Franklin</artist>
 * song :=   <song>Winter Wonderland</song>
 * similar to grep -t  but reads complete string until closing </tag>  
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int     BOOL;
#define FALSE   ((BOOL) 0)
#define TRUE    ((BOOL) 1)

#define BUF_SIZE        8192
#define TAG_SIZE          20
#define VAL_SIZE         100

const char grep_usage[] =
"usage:  getq [-s] [FILE]...\n"
"        search input file for <artist>Artist name</artist> and <song>song title</song>\n"
"        s=swap artist and song fields\n"
"        returns 0 if fields found, 1 otherwise.\n";

static   BOOL  search(char * value, const char * string, const char * tag);
static   BOOL  replace1(char * string, char search, char replace);
static   char *replace(char *orig, char *rep, char *with);
static   char *sanitize(char *orig);
static   BOOL  testing;

int main(int argc, char ** argv)
{
   FILE *      fp;
   const char *   name;
   const char *   cp;
   BOOL     swapFields;
   long     line;
   char     buf[BUF_SIZE];
   char     tag[TAG_SIZE];
   char     artist[VAL_SIZE];
   char     song[VAL_SIZE];
   BOOL     found;

   swapFields = FALSE;
   found    = FALSE;
   testing  = FALSE;

   argc--;
   argv++;
   if (argc < 1)
   {
      fprintf(stderr, "%s", grep_usage);
      return 1;
   }

   if (**argv == '-')
   {
      argc--;
      cp = *argv++;

      while (*++cp) switch (*cp)
      {
         case 's':
            swapFields = TRUE;
            break;

         default:
            fprintf(stderr, "Unknown option\n");
            return 1;
      }
   }

   while (argc-- > 0)
   {
      name = *argv++;
      fp = fopen(name, "r");
      if (fp == NULL)
      {
         perror(name);
         continue;
      }

      line = 0;
      while (fgets(buf, sizeof(buf), fp))
      {
         line++;
         cp = &buf[strlen(buf) - 1];
         if (*cp != '\n' && strlen(buf) == BUF_SIZE - 1) {
            fprintf(stderr, "%s: Line too long\n", name);
            return 1;
         }

         strcpy(tag, "song");
         if (search(song, buf, tag))
         {
            if (testing) printf("song is: %s\n", song);
            strcpy(tag, "artist");
            if (search(artist, buf, tag))
            {
               if (testing) printf("artist is: %s\n", artist);
               // replace blanks by + 
               replace1(song,' ', '+');
               replace1(artist,' ', '+');
               // replace non-XML characters
               char * const sanisong = sanitize(song);
               char * const saniartist = sanitize(artist);
               // swap fields if swapFields
               if (swapFields) 
                  printf("GET  /ws/2/recording?query=%s%%20AND%%20artist:%s&type:song&limit=1  HTTP/1.1\n", saniartist, sanisong);
               else 
                  printf("GET  /ws/2/recording?query=%s%%20AND%%20artist:%s&type:song&limit=1  HTTP/1.1\n", sanisong, saniartist);
               free(sanisong);
               free(saniartist);
               found = TRUE;

            }
         }
      }

      if (ferror(fp))
         perror(name);

      fclose(fp);
   }
   return found ? 0 : 1;
}


/*
 * See if the specified tag is found in the specified string.
 */
static BOOL search(char * value, const char * string, const char * tag)
{
   const char *   cp1;
   const char *   cp2;
   int      len;
   char     buf[strlen(tag)+4];

   len = strlen(tag);
   buf[0] = '<';
   strcpy(&buf[1],tag);
   buf[len+1] = '>';
   buf[len+2] = '\0';
   len +=2;
   if (testing) printf("tag = %s\n", buf);

   while (TRUE)
   {
      string = strchr(string, buf[0]);

      if (string == NULL)
         return FALSE;

      if (memcmp(string, buf, len) == 0) {
         cp1 = string+len;
         
         len = strlen(tag);
         buf[0] = '<';
         buf[1] = '/';
         strcpy(&buf[2],tag);
         buf[len+2] = '>';
         buf[len+3] = '\0';
         len +=3;
         if (testing) printf("tag = %s\n", buf);
         
         cp2 = strstr(cp1, buf);
         if (cp2 == NULL)
            return FALSE;
         strncpy(value,cp1, cp2 - cp1);
         return TRUE;
      }
      string++;
   }
}

/*
 * Replace one character by another one
 */
static BOOL replace1(char * string, char search, char replace)
{
   BOOL found = FALSE;
   for (int i = 0; i < strlen(string); i++) {
      if (string[i] == search) string[i] = replace;
      found = TRUE;
   }
   return found;
}


char *sanitize(char *orig) {
   char * newstr1 = replace(orig,    "+&", "");      // remove search term '&'
   char * newstr2 = replace(newstr1, "&", "&amp;");  // replace remaining ampersands
   free(newstr1);
   newstr1              = replace(newstr2, "<", "&lt;"); 
   free(newstr2);
   newstr2              = replace(newstr1, ">", "&gt;");
   free(newstr1); 
   newstr1              = replace(newstr2, "\"", "&quot;");
   free(newstr2);
   newstr2              = replace(newstr1, "\n", ""); 
   free(newstr1); 
   return newstr2;
}


// https://stackoverflow.com/a/779960/5124752
// You must free the result if result is non-NULL.
char *replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

/* END CODE */
