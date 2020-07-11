#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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

int main( int argc, char **argv )
{
	int dxvert = 0;
	int a = 1;
	if ( (argc > 1) && !strcmp(argv[a],"--dx") )
	{
		dxvert = 1;
		a++;
	}
	if ( argc < (5+dxvert) )
	{
		fprintf(stderr,"usage: vertsquish [--dx] <infile> <outfile>"
			" <firstframe-lastframe> <firstvert-lastvert>\n");
		return 1;
	}
	FILE *fin, *fout;
	if ( !(fin = fopen(argv[a],"rb")) )
	{
		fprintf(stderr,"Couldn't open input: %s\n",strerror(errno));
		return 2;
	}
	uint16_t nframes, fsize;
	fread(&nframes,2,1,fin);
	fread(&fsize,2,1,fin);
	uint8_t *framedata = malloc(nframes*fsize);
	fread(framedata,1,nframes*fsize,fin);
	fclose(fin);
	uint16_t framea = 0, frameb = 0;
	uint16_t verta = 0, vertb = 0;
	sscanf(argv[a+2],"%hu-%hu",&framea,&frameb);
	sscanf(argv[a+3],"%hu-%hu",&verta,&vertb);
	for ( int i=framea; i<=frameb; i++ )
	{
		int basep = i*fsize;
		// pass 1, calc midpoint
		int32_t midpoint[3] = {0};
		for ( int j=verta; j<=vertb; j++ )
		{
			if ( dxvert )
			{
				dxvert_t *dx = (dxvert_t*)(framedata+basep+j*8);
				midpoint[0] += dx->x;
				midpoint[1] += dx->y;
				midpoint[2] += dx->z;
			}
			else
			{
				uint32_t *uv = (uint32_t*)(framedata+basep+j*4);
				midpoint[0] += unpackuvert(*uv,0);
				midpoint[1] += unpackuvert(*uv,1);
				midpoint[2] += unpackuvert(*uv,2);
			}
		}
		midpoint[0] /= (vertb-verta)+1;
		midpoint[1] /= (vertb-verta)+1;
		midpoint[2] /= (vertb-verta)+1;
		// pass 2, squish into midpoint
		for ( int j=verta; j<=vertb; j++ )
		{
			if ( dxvert )
			{
				dxvert_t *dx = (dxvert_t*)(framedata+basep+j*8);
				dx->x = midpoint[0];
				dx->y = midpoint[1];
				dx->z = midpoint[2];
			}
			else
			{
				uint32_t *uv = (uint32_t*)(framedata+basep+j*4);
				*uv = packuvert(midpoint[0],midpoint[1],midpoint[2]);
			}
		}
	}
	if ( !(fout = fopen(argv[a+1],"wb")) )
	{
		fprintf(stderr,"Couldn't open output: %s\n",strerror(errno));
		free(framedata);
		return 2;
	}
	fwrite(&nframes,2,1,fout);
	fwrite(&fsize,2,1,fout);
	fwrite(framedata,1,nframes*fsize,fout);
	fclose(fout);
	free(framedata);
	return 0;
}
