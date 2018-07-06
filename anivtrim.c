#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main( int argc, char **argv )
{
	if ( argc < 5 )
	{
		fprintf(stderr,"usage: anivtrim <infile> <outfile>"
			" <start> <length>\n");
		return 1;
	}
	FILE *fin, *fout;
	if ( !(fin = fopen(argv[1],"rb")) )
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
	if ( !(fout = fopen(argv[2],"wb")) )
	{
		fprintf(stderr,"Couldn't open output: %s\n",strerror(errno));
		free(framedata);
		return 2;
	}
	int start = 0, num = 0;
	sscanf(argv[3],"%d",&start);
	sscanf(argv[4],"%d",&num);
	if ( num+start > nframes ) num = nframes-start;
	uint16_t nnframes = nframes-num;
	fwrite(&nnframes,2,1,fout);
	fwrite(&fsize,2,1,fout);
	int cursor = 0, j = 0;
	for ( int i=0; i<nframes; i++ )
	{
		if ( (i >= start) && (num-- > 0) ) printf("%d discarded\n",i);
		else
		{
			fwrite(framedata+cursor,1,fsize,fout);
			if ( i != j ) printf("%d is now %d\n",i,j);
			else printf("%d ignored\n",i);
			j++;
		}
		cursor += fsize;
	}
	fclose(fout);
	free(framedata);
	return 0;
}
