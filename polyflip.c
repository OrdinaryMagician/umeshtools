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

int *polylist = 0, npolys = 0;

void flip_poly( datapoly_t *p )
{
	uint16_t tmpvert = p->vertices[1];
	uint8_t tmpuv[2] = { p->uv[1][0], p->uv[1][1] };
	p->vertices[1] = p->vertices[2];
	p->uv[1][0] = p->uv[2][0];
	p->uv[1][1] = p->uv[2][1];
	p->vertices[2] = tmpvert;
	p->uv[2][0] = tmpuv[0];
	p->uv[2][1] = tmpuv[1];
}

int main( int argc, char **argv )
{
	FILE *datafile, *ndatafile;
	if ( argc < 4 )
	{
		fprintf(stderr,"usage: polyflip <infile> <outfile>"
			" <index> [index [...]]\n");
		return 1;
	}
	npolys = argc-3;
	polylist = calloc(npolys,sizeof(int));
	for ( int i=3; i<argc; i++ )
		sscanf(argv[i],"%d",&polylist[i-3]);
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
	for ( int i=0; i<npolys; i++ )
	{
		printf("Flipping poly %d\n",polylist[i]);
		flip_poly(&dpoly[polylist[i]]);
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
