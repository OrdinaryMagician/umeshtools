#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
	if ( argc < 5 )
	{
		fprintf(stderr,"usage: vertdedup <anivfilein> <datafilein>"
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
	uint64_t *dxvert = 0;
	uint32_t *avert = 0;
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
	if ( ahead.framesize/dhead.numverts == 8 )
	{
		dxvert = calloc(dhead.numverts*ahead.numframes,8);
		fread(dxvert,8,dhead.numverts*ahead.numframes,anivfile);
	}
	else if  ( ahead.framesize/dhead.numverts == 4 )
	{
		avert = calloc(dhead.numverts*ahead.numframes,4);
		fread(avert,4,dhead.numverts*ahead.numframes,anivfile);
	}
	else
	{
		fprintf(stderr,"Incorrect frame size %d, should be %u or %u."
			" Wrong anivfile?\n",ahead.framesize,dhead.numverts*4,
			dhead.numverts*8);
		free(dpoly);
		fclose(anivfile);
		return 16;
	}
	if ( feof(anivfile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		free(dpoly);
		if ( dxvert ) free(dxvert);
		else if ( avert ) free(avert);
		fclose(anivfile);
		return 8;
	}
	fclose(anivfile);
	int nremap = 0;
	int *remap = calloc(dhead.numverts,sizeof(int));
	for ( int i=0; i<dhead.numverts; i++ ) remap[i] = -1;
	for ( int i=0; i<dhead.numverts; i++ )
	{
		if ( remap[i] != -1 ) continue;
		for ( int j=i+1; j<dhead.numverts; j++ )
		{
			if ( remap[j] != -1 ) continue;
			int ident = 1;
			for ( int k=0; k<ahead.numframes; k++ )
			{
				int fs = k*dhead.numverts;
				if ( dxvert )
					ident = (dxvert[fs+i]==dxvert[fs+j]);
				else ident = (avert[fs+i]==avert[fs+j]);
				if ( !ident ) break;
			}
			if ( !ident ) continue;
			nremap++;
			remap[j] = i;
		}
	}
	if ( nremap )
	{
		printf("%d vertices to remap:\n",nremap);
		for ( int i=0; i<dhead.numverts; i++ )
			if ( remap[i] != -1 ) printf(" %d -> %d\n",i,remap[i]);
		printf("Remember to run umodeldiscard after this\n");
	}
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		for ( int j=0; j<3; j++ )
		{
			if ( remap[dpoly[i].vertices[j]] == -1 ) continue;
			dpoly[i].vertices[j] = remap[dpoly[i].vertices[j]];
		}
	}
	free(remap);
	if ( !(datafile = fopen(argv[4],"wb")) )
	{
		fprintf(stderr,"Failed to open output datafile: %s\n",
			strerror(errno));
		free(dpoly);
		if ( dxvert ) free(dxvert);
		else if ( avert ) free(avert);
		return 2;
	}
	fwrite(&dhead,sizeof(dataheader_t),1,datafile);
	fwrite(dpoly,sizeof(datapoly_t),dhead.numpolys,datafile);
	free(dpoly);
	fclose(datafile);
	if ( !(anivfile = fopen(argv[3],"wb")) )
	{
		fprintf(stderr,"Failed to open output anivfile: %s\n",
			strerror(errno));
		if ( dxvert ) free(dxvert);
		else if ( avert ) free(avert);
		return 2;
	}
	fwrite(&ahead,sizeof(aniheader_t),1,anivfile);
	if ( dxvert )
	{
		fwrite(dxvert,8,dhead.numverts*ahead.numframes,anivfile);
		free(dxvert);
	}
	else if ( avert )
	{
		fwrite(avert,4,dhead.numverts*ahead.numframes,anivfile);
		free(avert);
	}
	fclose(anivfile);
	return 0;
}
