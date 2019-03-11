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

uint16_t *polylist = 0;
int npolys = 0;
uint8_t typeset = 0, unset = 0;

void set_flag( datapoly_t *p )
{
	if ( unset ) p->type &= ~typeset;
	else p->type |= typeset;
}

int main( int argc, char **argv )
{
	FILE *datafile, *ndatafile;
	if ( argc < 4 )
	{
		fprintf(stderr,"usage: setumeshflag <infile> <outfile>"
			"[-]<type> <index[-index]> [index[-index] [...]]\n");
		return 1;
	}
	if ( argv[3][0] == '-' )
	{
		unset = 1;
		argv[3]++;
	}
	sscanf(argv[3],"%hhx",&typeset);
	// obtain lists
	for ( int i=4; i<argc; i++ )
	{
		// check for "all" wildcard
		if ( !strcmp(argv[i],"all") )
		{
			npolys = -1;	// means "all"
			break;
		}
		// check for range
		if ( !strchr(argv[i],'-') )
		{
			npolys++;
			if ( !polylist ) polylist = malloc(npolys*2);
			else polylist = realloc(polylist,npolys*2);
			sscanf(argv[i],"%hu",&polylist[npolys-1]);
			continue;
		}
		uint16_t min, max;
		sscanf(argv[i],"%hu-%hu",&min,&max);
		uint16_t num = (max-min)+1;
		int oldpos = npolys;
		npolys += num;
		if ( !polylist ) polylist = malloc(npolys*2);
		else polylist = realloc(polylist,npolys*2);
		uint16_t k = min;
		for ( int j=oldpos; j<npolys; j++ )
			polylist[j] = k++;
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
	fclose(datafile);
	if ( npolys == -1 )
	{
		for ( int i=0; i<dhead.numpolys; i++ )
		{
			printf("Processing poly %d\n",i);
			set_flag(&dpoly[i]);
		}
	}
	else
	{
		for ( int i=0; i<npolys; i++ )
		{
			printf("Processing poly %d\n",polylist[i]);
			set_flag(&dpoly[polylist[i]]);
		}
	}
	if ( !(ndatafile = fopen(argv[2],"wb")) )
	{
		fprintf(stderr,"Couldn't open output: %s\n",strerror(errno));
		free(dpoly);
		return 4;
	}
	fwrite(&dhead,sizeof(dataheader_t),1,ndatafile);
	fwrite(dpoly,sizeof(datapoly_t),dhead.numpolys,ndatafile);
	fclose(ndatafile);
	free(dpoly);
	return 0;
}
