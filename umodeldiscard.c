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
#define NORMAL_ONESIDED      0x00
#define NORMAL_TWOSIDED      0x01
#define TRANSLUCENT_TWOSIDED 0x02
#define MASKED_TWOSIDED      0x03
#define MODULATED_TWOSIDED   0x04
#define WEAPON_TRIANGLE      0x08
typedef struct
{
	uint16_t numframes;
	uint16_t framesize;
} __attribute__((packed)) aniheader_t;

int main( int argc, char **argv )
{
	FILE *datafile, *anivfile;
	FILE *ndatafile, *nanivfile;
	if ( argc < 5 )
	{
		fprintf(stderr,"usage: umodeldiscard <anivfilein> <datafilein>"
			" <anivfileout> <datafileout>\n");
		return 1;
	}
	if ( !(datafile = fopen(argv[2],"rb")) )
	{
		fprintf(stderr,"Failed to open input datafile: %s\n",
			strerror(errno));
		return 2;
	}
	dataheader_t dhead;
	datapoly_t *dpoly;
	aniheader_t ahead;
	uint8_t *avert;
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
	if ( !(anivfile = fopen(argv[1],"rb")) )
	{
		fprintf(stderr,"Failed to open input anivfile: %s\n",
			strerror(errno));
		return 2;
	}
	fread(&ahead,sizeof(aniheader_t),1,anivfile);
	if ( feof(anivfile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		free(dpoly);
		fclose(anivfile);
		return 8;
	}
	avert = malloc(ahead.numframes*ahead.framesize);
	fread(avert,ahead.framesize,ahead.numframes,anivfile);
	if ( feof(anivfile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		free(avert);
		free(dpoly);
		fclose(anivfile);
		return 8;
	}
	fclose(anivfile);
	// compute refcounts
	int *refcounts = calloc(dhead.numverts,sizeof(int));
	for ( int i=0; i<dhead.numpolys; i++ ) for ( int j=0; j<3; j++ )
		refcounts[dpoly[i].vertices[j]]++;
	printf("VERTEX REFERENCE COUNTERS:\n");
	for ( int i=0; i<dhead.numverts; i++ )
		printf(" %d: %d refs\n",i,refcounts[i]);
	// create remap list
	int *remap = calloc(dhead.numverts,sizeof(int));
	int c = 0;
	for ( int i=0; i<dhead.numverts; i++ )
	{
		if ( !refcounts[i] ) remap[i] = -1;
		else remap[i] = c++;
	}
	printf("VERTEX REMAP TABLE:\n");
	for ( int i=0; i<dhead.numverts; i++ )
		if ( remap[i] != -1 ) printf(" %d -> %d\n",i,remap[i]);
	// commence the rewrite
	if ( !(ndatafile = fopen(argv[4],"wb")) )
	{
		fprintf(stderr,"Failed to open output datafile: %s\n",
			strerror(errno));
		free(refcounts);
		free(remap);
		free(avert);
		free(dpoly);
		return 2;
	}
	dataheader_t nhead;
	memcpy(&nhead,&dhead,sizeof(dataheader_t));
	nhead.numverts = 0;
	for ( int i=0; i<dhead.numverts; i++ )
		if ( refcounts[i] ) nhead.numverts++;
	fwrite(&nhead,sizeof(dataheader_t),1,ndatafile);
	datapoly_t ndpoly;
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		memcpy(&ndpoly,&dpoly[i],sizeof(datapoly_t));
		for ( int j=0; j<3; j++ )
		{
			if ( remap[ndpoly.vertices[j]] == -1 )
				fprintf(stderr,"WARNING: remapped unreferenced"
					" vertex %d! This shouldn't happen!\n",
					ndpoly.vertices[j]);
			ndpoly.vertices[j] = remap[ndpoly.vertices[j]];
		}
		fwrite(&ndpoly,sizeof(datapoly_t),1,ndatafile);
	}
	fclose(ndatafile);
	free(dpoly);
	free(remap);
	if ( !(nanivfile = fopen(argv[3],"wb")) )
	{
		fprintf(stderr,"Failed to open output anivfile: %s\n",
			strerror(errno));
		free(refcounts);
		free(avert);
		return 2;
	}
	int vsize = ahead.framesize/dhead.numverts;
	if ( (vsize != 4) && (vsize != 8) )
	{
		fprintf(stderr,"Invalid vertex size %d. Should be 4 or 8.\n",
			vsize);
		free(refcounts);
		free(avert);
		fclose(nanivfile);
		return 16;
	}
	uint16_t nsize = 0;
	for ( int i=0; i<dhead.numverts; i++ )
		if ( refcounts[i] > 0 ) nsize += vsize;
	fwrite(&ahead.numframes,2,1,nanivfile);
	fwrite(&nsize,2,1,nanivfile);
	int cursor = 0;
	for ( int i=0; i<ahead.numframes; i++ )
	{
		for ( int j=0; j<dhead.numverts; j++ )
		{
			if ( refcounts[j] > 0 )
				fwrite(avert+cursor,vsize,1,nanivfile);
			cursor += vsize;
		}
	}
	free(refcounts);
	free(avert);
	fclose(nanivfile);
	return 0;
}
