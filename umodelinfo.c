#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

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

const char* typestr( uint8_t type )
{
	switch ( type )
	{
	case 0:
		return "normal";
	case 1:
		return "twosided";
	case 2:
		return "translucent";
	case 3:
		return "masked";
	case 4:
		return "modulated";
	case 8:
		return "weapon triangle";
	default:
		return "unknown";
	}
}

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

int main( int argc, char **argv )
{
	FILE *datafile, *anivfile;
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: umodelinfo <anivfile> <datafile>\n");
		return 1;
	}
	if ( !(datafile = fopen(argv[2],"rb")) )
	{
		fprintf(stderr,"Couldn't open datafile: %s\n",strerror(errno));
		return 2;
	}
	dataheader_t dhead;
	datapoly_t dpoly;
	aniheader_t ahead;
	uint32_t avert;
	int16_t dxvert[4];
	fread(&dhead,1,sizeof(dhead),datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(datafile));
		fclose(datafile);
		return 8;
	}
	printf("DATA HEADER\n %hu polys, %hu verts\n"
		" bogusrot %hu bogusframe %hu\n"
		" bogusnorm %u %u %u\n fixscale %u\n"
		" unused %u %u %u\n padding %hhu %hhu %hhu %hhu %hhu %hhu"
		" %hhu %hhu %hhu %hhu %hhu %hhu\n",dhead.numpolys,
		dhead.numverts,dhead.bogusrot,dhead.bogusframe,
		dhead.bogusnorm[0],dhead.bogusnorm[1],dhead.bogusnorm[2],
		dhead.fixscale,dhead.unused[0],dhead.unused[1],
		dhead.unused[2],dhead.padding[0],dhead.padding[1],
		dhead.padding[2],dhead.padding[3],dhead.padding[4],
		dhead.padding[5],dhead.padding[6],dhead.padding[7],
		dhead.padding[8],dhead.padding[9],dhead.padding[10],
		dhead.padding[11]);
	int *used = calloc(sizeof(int),256);
	int *refs = calloc(sizeof(int),dhead.numverts);
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		fread(&dpoly,1,sizeof(dpoly),datafile);
		if ( feof(datafile) )
		{
			free(used);
			free(refs);
			printf("Premature end of file reached at %lu\n",
				ftell(datafile));
			fclose(datafile);
			return 8;
		}
		printf("POLY %d\n vertices %hu %hu %hu\n"
			" type %s (%hhx)\n color %hhu\n"
			" uvs %hhu,%hhu %hhu,%hhu %hhu,%hhu\n"
			" texnum %hhx\n flags %hhx\n",i,dpoly.vertices[0],
			dpoly.vertices[1],dpoly.vertices[2],
			typestr(dpoly.type),dpoly.type,dpoly.color,
			dpoly.uv[0][0],dpoly.uv[0][1],dpoly.uv[1][0],
			dpoly.uv[1][1],dpoly.uv[2][0],dpoly.uv[2][1],
			dpoly.texnum,dpoly.flags);
		used[dpoly.texnum]++;
		refs[dpoly.vertices[0]]++;
		refs[dpoly.vertices[1]]++;
		refs[dpoly.vertices[2]]++;
	}
	if ( !feof(datafile) )
	{
		long pos = ftell(datafile);
		fseek(datafile,0,SEEK_END);
		long end = ftell(datafile);
		if ( (end-pos) > 0 )
			printf("Unused data detected at %ld: %ld bytes\n",
				pos,end-pos);
	}
	int ntex = 0;
	int nvert = 0;
	for ( int i=0; i<256; i++ ) if ( used[i] ) ntex++;
	for ( int i=0; i<dhead.numverts; i++ ) if ( refs[i] ) nvert++;
	printf("MISC STATS\n textures referenced: %d\n vertices referenced: %d"
		" (%d unreferenced)\n",ntex,nvert,dhead.numverts-nvert);
	if ( nvert < dhead.numverts )
	{
		printf("UNREFERENCED VERTICES:\n");
		for ( int i=0; i<dhead.numverts; i++ )
			if ( !refs[i] ) printf(" %d",i);
		printf("\n");
	}
	free(used);
	free(refs);
	fclose(datafile);
	if ( !(anivfile = fopen(argv[1],"rb")) )
	{
		fprintf(stderr,"Couldn't open anivfile: %s\n",strerror(errno));
		return 4;
	}
	fread(&ahead,1,sizeof(ahead),anivfile);
	if ( feof(anivfile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	printf("ANIM HEADER\n %hu frames of %hu bytes each",ahead.numframes,
		ahead.framesize);
	// check for Deus Ex's 16-bit vertex format
	int usedx;
	if ( (dhead.numverts*8) == ahead.framesize )
	{
		printf(" (Deus Ex format)\n");
		usedx = 1;
	}
	else if ( (dhead.numverts*4) == ahead.framesize )
	{
		printf(" (Standard format)\n");
		usedx = 0;
	}
	else
	{
		fprintf(stderr," (Unknown format)\nIncorrect frame size,"
			" should be %u or %u. Wrong anivfile?\n",
			dhead.numverts*4,dhead.numverts*8);
		fclose(anivfile);
		return 16;
	}
	int minx, miny, minz, maxx, maxy, maxz, midx, midy, midz, sizx,
		sizy, sizz;
	minx = miny = minz = INT_MAX;
	maxx = maxy = maxz = INT_MIN;
	for ( int i=0; i<ahead.numframes; i++ )
	{
		printf("FRAME %d\n",i);
		for ( int j=0; j<dhead.numverts; j++ )
		{
			if ( usedx ) fread(dxvert,4,sizeof(int16_t),anivfile);
			else fread(&avert,1,sizeof(avert),anivfile);
			if ( feof(anivfile) )
			{
				fprintf(stderr,"Premature end of file reached "
					"at %lu\n",ftell(anivfile));
				fclose(anivfile);
				return 8;
			}
			if ( usedx )
			{
				printf(" VERTEX %d\n  X %hd Y %hd Z %hd\n"
				"  raw value %04hx,%04hx,%04hx,%04hx\n",j,
				dxvert[0],dxvert[1],dxvert[2],dxvert[0],
				dxvert[1],dxvert[2],dxvert[3]);
				if ( dxvert[0] < minx ) minx = dxvert[0];
				if ( dxvert[1] < miny ) miny = dxvert[1];
				if ( dxvert[2] < minz ) minz = dxvert[2];
				if ( dxvert[0] > maxx ) maxx = dxvert[0];
				if ( dxvert[1] > maxy ) maxy = dxvert[1];
				if ( dxvert[2] > maxz ) maxz = dxvert[2];
			}
			else
			{	printf(" VERTEX %d\n  X %hd Y %hd Z %hd\n"
				"  raw value %08x\n",j,unpackuvert(avert,0),
				unpackuvert(avert,1),unpackuvert(avert,2),
				avert);
				if ( unpackuvert(avert,0) < minx )
					minx = unpackuvert(avert,0);
				if ( unpackuvert(avert,1) < miny )
					miny = unpackuvert(avert,1);
				if ( unpackuvert(avert,2) < minz )
					minz = unpackuvert(avert,2);
				if ( unpackuvert(avert,0) > maxx )
					maxx = unpackuvert(avert,0);
				if ( unpackuvert(avert,1) > maxy )
					maxy = unpackuvert(avert,1);
				if ( unpackuvert(avert,2) > maxz )
					maxz = unpackuvert(avert,2);
			}
		}
	}
	if ( !feof(anivfile) )
	{
		long pos = ftell(anivfile);
		fseek(anivfile,0,SEEK_END);
		long end = ftell(anivfile);
		if ( (end-pos) > 0 )
			printf("Unused data detected at %ld: %ld bytes\n",
				pos,end-pos);
	}
	midx = (minx+maxx)/2;
	midy = (miny+maxy)/2;
	midz = (minz+maxz)/2;
	sizx = maxx-minx;
	sizy = maxy-miny;
	sizz = maxz-minz;
	printf("MISC STATS\n minimum bounds: X %d Y %d Z %d\n"
		" maximum bounds: X %d Y %d Z %d\n estimated center:"
		" X %d Y %d Z %d\n total size: X %d Y %d Z %d\n",minx,miny,
		minz,maxx,maxy,maxz,midx,midy,midz,sizx,sizy,sizz);
	fclose(anivfile);
	return 0;
}
