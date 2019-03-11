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
// if true, texnums are sorted in reverse
// useful for transparency sorting of certain models,
// such as the health vials in UT
int inverse = 0;

int polysort( const void *a, const void *b )
{
	const datapoly_t *pa = a, *pb = b;
	// check if weapon triangle
	// this one always goes first
	if ( pa->type&8 && !(pb->type&8) )
		return -1;
	if ( !(pa->type&8) && pb->type&8 )
		return 1;
	// compare texture number
	if ( pa->texnum != pb->texnum )
		return inverse?(pb->texnum-pa->texnum):(pa->texnum-pb->texnum);
	// compare render style
	if ( (pa->type&7) != (pb->type&7) )
		return ((pa->type&7)-(pb->type&7));
	// compare other render flags
	if ( (pa->type&15) != (pb->type&15) )
		return ((pa->type&15)-(pb->type&15));
	// compare vertex indices
	for ( int i=0; i<3; i++ )
	{
		if ( pa->vertices[i] != pb->vertices[i] )
			return (pa->vertices[i]-pb->vertices[i]);
	}
	// treat as equal
	return 0;
}

int main( int argc, char **argv )
{
	FILE *datafile, *ndatafile;
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: polysort [-i] <datafilein>"
			" <datafileout>\n");
		return 1;
	}
	if ( !strcmp(argv[1],"-i") )
	{
		if ( argc < 4 )
		{
			fprintf(stderr,"usage: polysort [-i] <datafilein>"
				" <datafileout>\n");
			return 1;
		}
		inverse = 1;
		argv++;
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
	// sort the polys
	qsort(dpoly,dhead.numpolys,sizeof(datapoly_t),polysort);
	// write the new file
	if ( !(ndatafile = fopen(argv[2],"wb")) )
	{
		fprintf(stderr,"Failed to open output datafile: %s\n",
			strerror(errno));
		free(dpoly);
		return 2;
	}
	fwrite(&dhead,sizeof(dataheader_t),1,ndatafile);
	fwrite(dpoly,sizeof(datapoly_t),dhead.numpolys,ndatafile);
	free(dpoly);
	fclose(ndatafile);
	return 0;
}
