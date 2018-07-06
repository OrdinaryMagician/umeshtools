#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

int main( int argc, char **argv )
{
	FILE *datafile, *ndatafile;
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: texnumsq <datafilein> <datafileout>\n");
		return 1;
	}
	if ( !(datafile = fopen(argv[1],"rb")) )
	{
		fprintf(stderr,"Failed to open input datafile: %s\n",
			strerror(errno));
		return 2;
	}
	dataheader_t dhead;
	datapoly_t *dpoly;
	fread(&dhead,sizeof(dataheader_t),1,datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(datafile));
		fclose(datafile);
		return 8;
	}
	dpoly = calloc(dhead.numpolys,sizeof(datapoly_t));
	fread(dpoly,sizeof(datapoly_t),dhead.numpolys,datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(datafile));
		free(dpoly);
		fclose(datafile);
		return 8;
	}
	fclose(datafile);
	// compute refs
	uint8_t reftex[256] = {0};
	int remap[256] = {0};
	for ( int i=0; i<dhead.numpolys; i++ ) reftex[dpoly[i].texnum]++;
	printf("TEXNUM REFERENCE COUNTERS:\n");
	for ( int i=0; i<256; i++ )
		if ( reftex[i] ) printf(" %d: %d refs\n",i,reftex[i]);
	int j = 0;
	for ( int i=0; i<256; i++ )
	{
		if ( !reftex[i] ) remap[i] = -1;
		else remap[i] = j++;
	}
	printf("TEXNUM REMAP TABLE:\n");
	for ( int i=0; i<256; i++ )
		if ( remap[i] != -1 ) printf(" %d -> %d\n",i,remap[i]);
	// commence rewrite
	if ( !(ndatafile = fopen(argv[2],"wb")) )
	{
		fprintf(stderr,"Failed to open output datafile: %s\n",
			strerror(errno));
		return 2;
	}
	fwrite(&dhead,sizeof(dataheader_t),1,ndatafile);
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		if ( remap[dpoly[i].texnum] == -1 )
			fprintf(stderr,"WARNING: remapped unreferenced"
				" texture %d! This shouldn't happen!\n",
				dpoly[i].texnum);
		dpoly[i].texnum = remap[dpoly[i].texnum];
	}
	fwrite(dpoly,sizeof(datapoly_t),dhead.numpolys,ndatafile);
	fclose(ndatafile);
	return 0;
}
