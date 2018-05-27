#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

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
	uint32_t out = 0;
	out |= ((z>>6)&0x3ff)<<22;
	out |= ((y>>5)&0x7ff)<<11;
	out |= (x>>5)&0x7ff;
	return out;
}

int main( int argc, char **argv )
{
	if ( argc < 4 )
	{
		fprintf(stderr,
			"usage: dxconv todx/fromdx <infile> <outfile>\n");
		return 1;
	}
	FILE *fin, *fout;
	aniheader_t hin, hout;
	uint32_t uvert = 0;
	int16_t dxvert[4] = {0};
	int mode = 0;
	if ( !strcmp(argv[1],"fromdx") ) mode = 1;
	if ( !(fin = fopen(argv[2],"rb")) )
	{
		fprintf(stderr,"Couldn't open input: %s\n",strerror(errno));
		return 2;
	}
	if ( !(fout = fopen(argv[3],"wb")) )
	{
		fprintf(stderr,"Couldn't open output: %s\n",strerror(errno));
		return 2;
	}
	fread(&hin,sizeof(aniheader_t),1,fin);
	if ( !hin.numframes || !hin.framesize )
	{
		fprintf(stderr,"Invalid file: zero numframes/framesize\n");
		fclose(fin);
		fclose(fout);
		return 4;
	}
	if ( mode && hin.framesize%8 )
	{
		fprintf(stderr,
			"Frame size not divisible by 8. Not a DX model?\n");
		fclose(fin);
		fclose(fout);
		return 8;
	}
	else if ( hin.framesize%4 )
	{
		fprintf(stderr,
			"Frame size not divisible by 4. Not an UE1 model?\n");
		fclose(fin);
		fclose(fout);
		return 8;
	}
	hout.numframes = hin.numframes;
	hout.framesize = mode?(hin.framesize/2):(hin.framesize*2);
	fwrite(&hout,sizeof(aniheader_t),1,fout);
	int nverts = mode?(hin.framesize/8):(hin.framesize/4);
	for ( int i=0; i<nverts; i++ )
	{
		if ( mode )
		{
			fread(dxvert,8,1,fin);
			uvert = packuvert(dxvert[0],dxvert[1],dxvert[2]);
			fwrite(&uvert,4,1,fout);
		}
		else
		{
			fread(&uvert,4,1,fin);
			dxvert[0] = unpackuvert(uvert,0);
			dxvert[1] = unpackuvert(uvert,1);
			dxvert[2] = unpackuvert(uvert,2);
			fwrite(dxvert,8,1,fout);
		}
	}
	fclose(fin);
	fclose(fout);
	return 0;
}
