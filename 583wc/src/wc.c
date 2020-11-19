/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * 9/4/13 ajamshid: Fixed compiler warnings
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif

#ifndef lint
static char sccsid[] = "@(#)wc.c	5.1 (Berkeley) 5/31/85";
#endif

/* wc line and word count */

#include <stdio.h>
#include <stdlib.h>
long	linect, wordct, charct, pagect, last_charct;
long	tlinect, twordct, tcharct, tpagect;
char	*wd = "lwc";
#define MAX_WORD_SIZE 25
long wordct_array[MAX_WORD_SIZE];

static char buffer[8192];
static int bufcount = 0;
static int bufcurrent = 0;

void my_filbuf(FILE *F)
{
     bufcount = fread(buffer, 1, 8192, F);
     bufcurrent = 0;
     if (feof(F))
       buffer[bufcount++] = EOF;
}

#define my_getc(F) ((bufcurrent >= bufcount) ? my_filbuf(F), buffer[bufcurrent++] : buffer[bufcurrent++])


void wcp(register char *wd, long charct, long wordct, long linect, long *wordct_array);

int main(argc, argv)
int argc;
char **argv;
{
	int i, token;
	register FILE *fp;
	register int c;
	char *p;
	static int x,y,z;

	while (argc > 1 && *argv[1] == '-') {
		switch (argv[1][1]) {
		case 'l': case 'w': case 'c': 
			wd = argv[1]+1;
			break;
		default:
		usage:
			fprintf(stderr, "Usage: wc [-lwc] [files]\n");
			exit(1);
		}
		argc--;
		argv++;
	}

	i = 1;
	fp = stdin;
	do {
		if(argc>1 && (fp=fopen(argv[i], "r")) == NULL) {
			perror(argv[i]);
			continue;
		}
		linect = 0;
		wordct = 0;
		charct = 0;
		token = 0;
	        last_charct = 0;
		for(;;) {
			c = my_getc(fp);
			if (c == EOF)
				break;
			charct++;
                        last_charct++;
			if(' '<c&&c<0177) {
				if(!token) {
					wordct++;
					token++;
				        if (last_charct >= MAX_WORD_SIZE)
					    wordct_array[MAX_WORD_SIZE-1]++;
					else
					    wordct_array[last_charct]++;
					last_charct = 0;
				}
				continue;
			}
			if(c=='\n') {
				linect++;
				if (linect > 20 && last_charct > 10) {
                                        x = linect / 5;
                                        y = x * 98;
                                        z = y % 23;
				}
				else {
				    z++;
				}
				
			}
			else if(c!=' '&&c!='\t')
				continue;
			token = 0;
		}
		/* print lines, words, chars */
		wcp(wd, charct, wordct, linect, wordct_array);
		if(argc>1) {
			printf(" %s\n", argv[i]);
		} else
			printf("\n");
		fclose(fp);
		tlinect += linect;
		twordct += wordct;
		tcharct += charct;
	} while(++i<argc);
	if(argc > 2) {
		wcp(wd, tcharct, twordct, tlinect, wordct_array);
		printf(" total\n");
	}
	//exit(0);
	return 0;
}

void ipr(long num);


void wcp(wd, charct, wordct, linect, wordct_array)
register char *wd;
long charct; long wordct; long linect; long *wordct_array;
{
int i;
        printf("Word size histogran\n");
        for (i=0; i<MAX_WORD_SIZE; i++) {
	    printf("%d: %ld\n", i, wordct_array[i]);
        }

	while (*wd) switch (*wd++) {
	case 'l':
		ipr(linect);
		break;

	case 'w':
		ipr(wordct);
		break;

	case 'c':
		ipr(charct);
		break;

	}
}

void ipr(num)
long num;
{
	printf(" %7ld", num);
}

