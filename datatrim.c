#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

typedef struct
{
	uint16_t numpolys;
	uint16_t numverts;
	/* everything below is unused */
	uint16_t bogusrot;
	uint16_t bogusframe;
	uint32_t bogusnorm[3];
	uint32_t fixscale;
	uint32_t unused[3];
	uint8_t padding[12];
} __attribute__((packed)) dataheader_t;
typedef struct
{
	uint16_t vertices[3];
	uint8_t type;
	uint8_t color; /* unused */
	uint8_t uv[3][2];
	uint8_t texnum;
	uint8_t flags; /* unused */
} __attribute__((packed)) datapoly_t;
#define NORMAL_ONESIDED      0x00
#define NORMAL_TWOSIDED      0x01
#define TRANSLUCENT_TWOSIDED 0x02
#define MASKED_TWOSIDED      0x03
#define MODULATED_TWOSIDED   0x04
#define WEAPON_TRIANGLE      0x08

#define DISCARD_INUV   1
#define DISCARD_OUTUV  2
#define DISCARD_ISNUM  4
#define DISCARD_NOTNUM 8
int discardmode = 0;
uint8_t discardminu = 0,
    discardmaxu = 0,
    discardminv = 0,
    discardmaxv = 0,
    discardtexn = 0;

int discard( datapoly_t *p )
{
	if ( discardmode&DISCARD_INUV )
	{
		for ( int i=0; i<3; i++ )
		{
			if ( (p->uv[i][0] >= discardminu)
				&& (p->uv[i][0] <= discardmaxu)
				&& (p->uv[i][1] >= discardminv)
				&& (p->uv[i][1] <= discardmaxv) )
				return 1;
		}
	}
	if ( discardmode&DISCARD_OUTUV )
	{
		for ( int i=0; i<3; i++ )
		{
			if ( (p->uv[i][0] < discardminu)
				|| (p->uv[i][0] > discardmaxu)
				|| (p->uv[i][1] < discardminv)
				|| (p->uv[i][1] > discardmaxv) )
				return 1;
		}
	}
	if ( discardmode&DISCARD_ISNUM && (p->texnum == discardtexn) )
		return 1;
	if ( discardmode&DISCARD_NOTNUM && (p->texnum != discardtexn) )
		return 1;
	return 0;
}

int main( int argc, char **argv )
{
	FILE *datafile, *ndatafile;
	if ( argc < 4 )
	{
		fprintf(stderr,"usage: datatrim <infile> <outfile>"
			" <commands>\n\ncommand reference:\n - inuv x1 y1"
			" x2 y2 : trim out triangles with tex coords within"
			" specified range (inclusive)\n - outuv x1 y1 x2 y2 :"
			" trim out triangles with tex coords outside specified"
			" range\n - isnum n : trim out triangles with specified"
			" texture number\n - notnum n : trim out triangles"
			" without specified texture number\n");
		return 1;
	}
	for ( int i=3; i<argc; i++ )
	{
		if ( !strcmp(argv[i],"inuv") )
		{
			if ( argc < i+4 )
			{
				fprintf(stderr,"inuv expects 4 parameters\n");
				return 1;
			}
			discardmode |= DISCARD_INUV;
			sscanf(argv[++i],"%hhu",&discardminu);
			sscanf(argv[++i],"%hhu",&discardmaxu);
			sscanf(argv[++i],"%hhu",&discardminv);
			sscanf(argv[++i],"%hhu",&discardmaxv);
		}
		else if ( !strcmp(argv[i],"outuv") )
		{
			if ( argc < i+4 ) return 1;
			{
				fprintf(stderr,"outuv expects 4 parameters\n");
				return 1;
			}
			discardmode |= DISCARD_OUTUV;
			sscanf(argv[++i],"%hhu",&discardminu);
			sscanf(argv[++i],"%hhu",&discardmaxu);
			sscanf(argv[++i],"%hhu",&discardminv);
			sscanf(argv[++i],"%hhu",&discardmaxv);
		}
		else if ( !strcmp(argv[i],"isnum") )
		{
			if ( argc < i+1 ) return 1;
			{
				fprintf(stderr,"isnum expects 1 parameter\n");
				return 1;
			}
			discardmode |= DISCARD_ISNUM;
			sscanf(argv[++i],"%hhu",&discardtexn);
		}
		else if ( !strcmp(argv[i],"notnum") )
		{
			if ( argc < i+1 ) return 1;
			{
				fprintf(stderr,"notnum expects 1 parameter\n");
				return 1;
			}
			discardmode |= DISCARD_NOTNUM;
			sscanf(argv[++i],"%hhu",&discardtexn);
		}
	}
	if ( !(datafile = fopen(argv[1],"rb")) )
	{
		fprintf(stderr,"Couldn't open input: %s\n",strerror(errno));
		return 2;
	}
	dataheader_t dhead;
	fread(&dhead,sizeof(dataheader_t),1,datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"--Premature end of file reached at %lu--\n",
			ftell(datafile));
		fclose(datafile);
		return 8;
	}
	datapoly_t *dpoly = calloc(dhead.numpolys,sizeof(datapoly_t));
	fread(dpoly,sizeof(datapoly_t),dhead.numpolys,datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"--Premature end of file reached at %lu--\n",
			ftell(datafile));
		fclose(datafile);
		free(dpoly);
		return 8;
	}
	if ( !(ndatafile = fopen(argv[2],"wb")) )
	{
		fprintf(stderr,"Couldn't open output: %s\n",strerror(errno));
		free(dpoly);
		return 4;
	}
	dataheader_t ndhead;
	memcpy(&ndhead,&dhead,sizeof(dataheader_t));
	ndhead.numpolys = 0;
	// first pass, count discarded
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		if ( discard(&dpoly[i]) )
		{
			printf("discarded poly %d\n",i);
			continue;
		}
		ndhead.numpolys++;
	}
	fwrite(&ndhead,sizeof(dataheader_t),1,ndatafile);
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		if ( discard(&dpoly[i]) ) continue;
		fwrite(&dpoly[i],sizeof(datapoly_t),1,ndatafile);
	}
	fclose(datafile);
	free(dpoly);
	return 0;
}
