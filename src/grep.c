/*
 * Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 *
 * The "grep" command, taken from sash.
 * This provides basic file searching.
 *
 * Permission to distribute this code under the GPL has been granted.
 * Modified for busybox by Erik Andersen <andersee@debian.org> <andersen@lineo.com>
 * Modified for internet radio scripting by Thomas H. Schmidt <Thomas.Heinrich.Schmidt@googlemail.com>
 */
 
 /* https://elixir.bootlin.com/busybox/0.29alpha2/source/grep.c */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int     BOOL;
#define FALSE   ((BOOL) 0)
#define TRUE    ((BOOL) 1)
#define MIN(X,Y) (((X) < (Y)) ? (X) : (Y))

#define BUF_SIZE        8192*2
#define TAG_SIZE        100

const char grep_usage[] =
"usage:  grep [-inqst] PATTERN [FILES]...\n"
"        Search input file(s) for lines matching the given pattern.\n"
"        Searching stdin if no files are given.\n"
"        No regular expressions.\n"
"        i=ignore case, n=list line numbers, q=quiet\n"
"        t=use (XML) tag <PATTERN>, print tag value, respect case\n"
"        s=a numerical tag value is divided by 1000\n"
"        Returns 0 (TRUE) if found, 1 otherwise.\n";

static   BOOL  search(const char * string, const char * word, BOOL ignoreCase, BOOL parseTag, BOOL scaleNumber, double scalingFactor);

int main(int argc, char ** argv)
{
   FILE *      fp;
   const char *   word;
   const char *   name;
   const char *   cp;
   BOOL     tellName;
   BOOL     ignoreCase;
   BOOL     tellLineNo;
   BOOL     parseTag;
   BOOL     scaleNumber;
   BOOL     quiet;
   long     line;
   double   scalingFactor;
   char     buf[BUF_SIZE];
   BOOL     found;
   BOOL     lineTooLong;

   ignoreCase = FALSE;
   tellLineNo = FALSE;
   parseTag = FALSE;
   scaleNumber = FALSE;
   quiet    = FALSE;
   found    = FALSE;
   lineTooLong = FALSE;

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
         case 'i':
            ignoreCase = TRUE;
            break;

         case 'n':
            tellLineNo = TRUE;
            break;

         case 't':
            parseTag = TRUE;
            ignoreCase = FALSE;
            quiet = TRUE;
            break;
            
         case 's':
            scaleNumber = TRUE;
            scalingFactor = 1.0/1000.0;
            break;

         case 'q':
            quiet = TRUE;
            break;

         default:
            fprintf(stderr, "Unknown option\n");
            return 1;
      }
   }

   word = *argv++;
   argc--;

   tellName = (argc > 1);

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
            lineTooLong = TRUE;
            if (!parseTag) fprintf(stderr, "%s: Line too long\n", name);
         }

         if (search(buf, word, ignoreCase, parseTag, scaleNumber, scalingFactor))
         {
            if (tellName && !quiet)
               printf("%s: ", name);

            if (tellLineNo && !quiet)
               printf("%ld: ", line);

            if (!quiet) fputs(buf, stdout);
            found = TRUE;
            if (parseTag && lineTooLong) break;
         }
      }

      if (ferror(fp))
         perror(name);

      fclose(fp);
   }
   return found ? 0 : 1;
}


/*
 * See if the specified word is found in the specified string.
 */
static BOOL search(const char * string, const char * word, BOOL ignoreCase, BOOL parseTag, BOOL scaleNumber, double scalingFactor)
{
   const char *   cp1;
   const char *   cp2;
   int      len;
   int      lowFirst;
   int      ch1;
   int      ch2;
   long int value;
   char     buf[strlen(word)+3];

   len = strlen(word);
   if (parseTag) {
      buf[0] = '<';
      strcpy(&buf[1],word);
      buf[len+1] = '>';
      buf[len+2] = '\0';
      word = buf;
      len +=2;
      // printf("word = %s\n", word);
   }

   if (!ignoreCase)
   {
      while (TRUE)
      {
         string = strchr(string, word[0]);

         if (string == NULL)
            return FALSE;

         if (memcmp(string, word, len) == 0) {
            if (parseTag) {
               // printf("tag value = %s\n", string+len);
               BOOL isNumber = TRUE;
               char tagValue[TAG_SIZE+1];
               char *pc;
               for (pc = (char *)(string+len); *pc !=0; pc++) {
                  if (*pc == '<') break;
                  if (! isdigit(*pc)) isNumber = FALSE;
                  if ((pc - (string+len)) < TAG_SIZE) {
                     tagValue[pc - (string+len)] = *pc;
                     // printf("set tagValue[%d] = %c\n", pc - (string+len), *pc);
                  }
               }
               tagValue[MIN(pc - (string+len),TAG_SIZE)] = '\0';
               // printf("tag value = %s, isNumber = %d, length = %d, last char pos = %d\n", tagValue, isNumber, pc - (string+len), MIN(pc - (string+len),TAG_SIZE));
               if (isNumber) {
                  sscanf(string+len,"%ld",&value);
                  if (scaleNumber) {
                     long v = value*scalingFactor;
                     printf("%ld", v);
                  }
                  else {
                     printf("%ld", value);
                  }
               }
               else {
                  printf("%s", tagValue);
               }
            }
            return TRUE;
         }

         string++;
      }
   }

   /*
    * Here if we need to check case independence.
    * Do the search by lower casing both strings.
    */
   lowFirst = *word;

   if (isupper(lowFirst))
      lowFirst = tolower(lowFirst);

   while (TRUE)
   {
      while (*string && (*string != lowFirst) &&
         (!isupper(*string) || (tolower(*string) != lowFirst)))
      {
         string++;
      }

      if (*string == '\0')
         return FALSE;

      cp1 = string;
      cp2 = word;

      do
      {
         if (*cp2 == '\0')
            return TRUE;

         ch1 = *cp1++;

         if (isupper(ch1))
            ch1 = tolower(ch1);

         ch2 = *cp2++;

         if (isupper(ch2))
            ch2 = tolower(ch2);

      }
      while (ch1 == ch2);

      string++;
   }
}

/* END CODE */
