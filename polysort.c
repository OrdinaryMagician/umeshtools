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

#define PT_NORMAL        0
#define PT_TWOSIDED      1
#define PT_TRANSLUCENT   2
#define PT_MASKED        3
#define PT_MODULATED     4
#define PT_227ALPHABLEND 5
#define PT_UNUSED1       6
#define PT_UNUSED2       7
#define PT_MASK          0x07
#define PF_SPECIALTRI    0x08
#define PF_UNLIT         0x10
#define PF_CURVY         0x20
#define PF_227FLATSHADED 0x20
#define PF_MESHENVIROMAP 0x40
#define PF_NOSMOOTH      0x80
#define PF_MASK          0xF8

int polysort( const void *a, const void *b )
{
	const datapoly_t *pa = a, *pb = b;
	// check if weapon triangle
	// this one always goes first
	if ( pa->type&PF_SPECIALTRI && !(pb->type&PF_SPECIALTRI) )
		return -1;
	if ( !(pa->type&PF_SPECIALTRI) && pb->type&PF_SPECIALTRI )
		return 1;
	// compare texture number
	if ( pa->texnum != pb->texnum )
		return inverse?(pb->texnum-pa->texnum):(pa->texnum-pb->texnum);
	int ord[8] = {0,1,3,4,5,2,6,7};
	// compare render style
	if ( (pa->type&PT_MASK) != (pb->type&PT_MASK) )
		return (ord[(pa->type&PT_MASK)]-ord[(pb->type&PT_MASK)]);
	// compare other render flags
	if ( (pa->type&PF_MASK) != (pb->type&PF_MASK) )
		return ((pa->type&PF_MASK)-(pb->type&PF_MASK));
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
