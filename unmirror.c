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
typedef struct
{
	uint16_t numframes;
	uint16_t framesize;
} __attribute__((packed)) aniheader_t;
int16_t unpackuvert( uint32_t v, int c )
{
	switch ( c )
	{
	case 0:
		return (v&0x7ff)<<5;
	case 1:
		return ((v>>11)&0x7ff)<<5;
	case 2:
		return ((v>>22)&0x3ff)<<6;
	default:
		return 0;
	}
}
uint32_t packuvert( int16_t x, int16_t y, int16_t z )
{
	uint32_t uvert = ((z>>6)&0x3ff)<<22;
	uvert |= ((y>>5)&0x7ff)<<11;
	uvert |= ((x>>5)&0x7ff);
	return uvert;
}
typedef struct
{
	int16_t x, y, z, pad;
} __attribute__((packed)) dxvert_t;

dataheader_t dhead;
datapoly_t *dpoly;
aniheader_t ahead;
uint32_t *avert;
dxvert_t *dxvert;

int main( int argc, char **argv )
{
	if ( argc < 5 )
	{
		fprintf(stderr,"usage: unmirror <anivfilein> <datafilein>"
			" <anivfileout> <datafileout>\n");
		return 1;
	}
	FILE *datafile, *anivfile;
	// read datafile
	if ( !(datafile = fopen(argv[2],"rb")) )
	{
		fprintf(stderr,"Cannot open datafile '%s': %s\n",argv[2],strerror(errno));
		return 2;
	}
	fread(&dhead,1,sizeof(dataheader_t),datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[2],ftell(datafile));
		fclose(datafile);
		return 8;
	}
	dpoly = calloc(dhead.numpolys,sizeof(datapoly_t));
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		fread(&dpoly[i],1,sizeof(datapoly_t),datafile);
		if ( feof(datafile) )
		{
			free(dpoly);
			fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[2],ftell(datafile));
			fclose(datafile);
			return 8;
		}
	}
	fclose(datafile);
	// read anivfile
	if ( !(anivfile = fopen(argv[1],"rb")) )
	{
		free(dpoly);
		fprintf(stderr,"Cannot open anivfile '%s': %s\n",argv[1],strerror(errno));
		return 2;
	}
	fread(&ahead,1,sizeof(aniheader_t),anivfile);
	if ( feof(anivfile) )
	{
		free(dpoly);
		fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[1],ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	// check for Deus Ex's 16-bit vertex format
	int usedx;
	if ( (dhead.numverts*8) == ahead.framesize )
	{
		usedx = 1;
		avert = 0;
		dxvert = calloc(dhead.numverts*ahead.numframes,sizeof(dxvert_t));
	}
	else if ( (dhead.numverts*4) == ahead.framesize )
	{
		usedx = 0;
		avert = calloc(dhead.numverts*ahead.numframes,sizeof(uint32_t));
		dxvert = 0;
	}
	else
	{
		free(dpoly);
		fprintf(stderr,"Incorrect frame size %u in '%s', should be %u or %u. Wrong anivfile?\n",ahead.framesize,argv[1],dhead.numverts*4,dhead.numverts*8);
		fclose(anivfile);
		return 16;
	}
	for ( int i=0; i<(dhead.numverts*ahead.numframes); i++ )
	{
		if ( usedx ) fread(&dxvert[i],1,sizeof(dxvert_t),anivfile);
		else fread(&avert[i],1,sizeof(uint32_t),anivfile);
		if ( feof(anivfile) )
		{
			free(dpoly);
			if ( dxvert ) free(dxvert);
			else free(avert);
			fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[1],ftell(anivfile));
			fclose(anivfile);
			return 8;
		}
	}
	fclose(anivfile);
	// change triangle winding
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		int tmp = dpoly[i].vertices[1];
		dpoly[i].vertices[1] = dpoly[i].vertices[2];
		dpoly[i].vertices[2] = tmp;
		tmp = dpoly[i].uv[1][0];
		dpoly[i].uv[1][0] = dpoly[i].uv[2][0];
		dpoly[i].uv[2][0] = tmp;
		tmp = dpoly[i].uv[1][1];
		dpoly[i].uv[1][1] = dpoly[i].uv[2][1];
		dpoly[i].uv[2][1] = tmp;
	}
	// transform vertices
	for ( int i=0; i<(dhead.numverts*ahead.numframes); i++ )
	{
		if ( dxvert ) dxvert[i].x *= -1;
		else
		{
			int16_t x, y, z;
			x = unpackuvert(avert[i],0);
			y = unpackuvert(avert[i],1);
			z = unpackuvert(avert[i],2);
			x *= -1;
			avert[i] = packuvert(x,y,z);
		}
	}
	// write output files
	if ( !(datafile = fopen(argv[4],"wb")) )
	{
		free(dpoly);
		if ( dxvert ) free(dxvert);
		else free(avert);
		fprintf(stderr,"Cannot open datafile '%s': %s\n",argv[4],strerror(errno));
		return 4;
	}
	fwrite(&dhead,1,sizeof(dataheader_t),datafile);
	fwrite(dpoly,dhead.numpolys,sizeof(datapoly_t),datafile);
	fclose(datafile);
	if ( !(anivfile = fopen(argv[3],"wb")) )
	{
		free(dpoly);
		if ( dxvert ) free(dxvert);
		else free(avert);
		fprintf(stderr,"Cannot open anivfile '%s': %s\n",argv[3],strerror(errno));
		return 4;
	}
	fwrite(&ahead,1,sizeof(aniheader_t),anivfile);
	if ( dxvert ) fwrite(dxvert,1,ahead.numframes*ahead.framesize,anivfile);
	else fwrite(avert,1,ahead.numframes*ahead.framesize,anivfile);
	fclose(anivfile);
	free(dpoly);
	if ( dxvert ) free(dxvert);
	else free(avert);
	return 0;
}
