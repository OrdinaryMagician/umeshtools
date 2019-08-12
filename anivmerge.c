#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: anivtrim <outfile> <infile1>"
			" [infile2 ...]\n");
		return 1;
	}
	FILE *fin, *fout;
	if ( !(fin = fopen(argv[2],"rb")) )
	{
		fprintf(stderr,"Couldn't open '%s': %s\n",argv[2],
			strerror(errno));
		return 2;
	}
	uint16_t nframes, fsize;
	fread(&nframes,2,1,fin);
	fread(&fsize,2,1,fin);
	uint8_t *framedata = malloc(nframes*fsize);
	fread(framedata,1,nframes*fsize,fin);
	fclose(fin);
	for ( int i=3; i<argc; i++ )
	{
		if ( !(fin = fopen(argv[i],"rb")) )
		{
			fprintf(stderr,"Couldn't open '%s': %s\n",argv[i],
				strerror(errno));
			free(framedata);
			return 2;
		}
		uint16_t nframes2, fsize2;
		fread(&nframes2,2,1,fin);
		fread(&fsize2,2,1,fin);
		if ( fsize2 != fsize )
		{
			fprintf(stderr,"Cannot merge: '%s' has a different frame"
				" size than '%s' (%hu != %hu).\n",
				argv[i],argv[2],fsize2,fsize);
			fclose(fin);
			free(framedata);
			return 4;
		}
		framedata = realloc(framedata,(nframes+nframes2)*fsize);
		fread(framedata+nframes*fsize,1,nframes2*fsize,fin);
		fclose(fin);
		nframes += nframes2;
	}
	if ( !(fout = fopen(argv[1],"wb")) )
	{
		fprintf(stderr,"Couldn't open '%s': %s\n",argv[1],
			strerror(errno));
		free(framedata);
		return 2;
	}
	fwrite(&nframes,2,1,fout);
	fwrite(&fsize,2,1,fout);
	fwrite(framedata,1,nframes*fsize,fout);
	free(framedata);
	return 0;
}
