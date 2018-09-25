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

typedef struct
{
	float x, y, z;
} vert_t;
typedef struct
{
	float u, v;
} coord_t;
typedef struct
{
	int v[3], c[3];
	int mat;
} face_t;
typedef struct
{
	int *faces;
	int nface;
	int mat;
} group_t;

int main( int argc, char **argv )
{
	FILE *datafile, *anivfile;
	if ( argc < 4 )
	{
		fprintf(stderr,"usage: umesh2obj <anivfile> <datafile>"
			" <prefix>\n");
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
	vert_t *verts = calloc(dhead.numverts,sizeof(vert_t));
	coord_t *coords = calloc(dhead.numpolys*3,sizeof(coord_t));
	face_t *faces  = calloc(dhead.numpolys,sizeof(face_t));
	group_t *groups = malloc(0);
	int nvert = dhead.numverts, ncoord = dhead.numpolys*3,
		nface = dhead.numpolys, ngroup = 0;
	// populate coord and face lists
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		fread(&dpoly,1,sizeof(datapoly_t),datafile);
		if ( feof(datafile) )
		{
			free(verts);
			free(coords);
			free(faces);
			free(groups);
			printf("Premature end of file reached at %lu\n",
				ftell(datafile));
			fclose(datafile);
			return 8;
		}
		coords[i*3].u = dpoly.uv[0][0]/255.f;
		coords[i*3].v = 1.f-dpoly.uv[0][1]/255.f;
		coords[i*3+1].u = dpoly.uv[1][0]/255.f;
		coords[i*3+1].v = 1.f-dpoly.uv[1][1]/255.f;
		coords[i*3+2].u = dpoly.uv[2][0]/255.f;
		coords[i*3+2].v = 1.f-dpoly.uv[2][1]/255.f;
		faces[i].v[0] = dpoly.vertices[0];
		faces[i].v[1] = dpoly.vertices[1];
		faces[i].v[2] = dpoly.vertices[2];
		faces[i].c[0] = i*3;
		faces[i].c[1] = i*3+1;
		faces[i].c[2] = i*3+2;
		faces[i].mat = dpoly.texnum;
	}
	fclose(datafile);
	// populate groups
	int c = -1;
	for ( int i=0; i<nface; i++ )
	{
		if ( c == -1 )
		{
			c = ngroup++;
			groups = realloc(groups,ngroup*sizeof(group_t));
			groups[c].faces = malloc(0);
			groups[c].nface = 0;
			groups[c].mat = faces[i].mat;
		}
		else if ( faces[i].mat != groups[c].mat )
		{
			c = -1;
			for ( int j=0; j<ngroup; j++ )
			{
				if ( groups[j].mat != faces[i].mat )
					continue;
				c = j;
				break;
			}
			i--;
			continue;
		}
		groups[c].faces = realloc(groups[c].faces,
			sizeof(int)*(groups[c].nface+1));
		groups[c].faces[groups[c].nface++] = i;
	}
	if ( !(anivfile = fopen(argv[1],"rb")) )
	{
		free(verts);
		free(coords);
		free(faces);
		for ( int i=0; i<ngroup; i++ ) free(groups[i].faces);
		free(groups);
		fprintf(stderr,"Couldn't open anivfile: %s\n",strerror(errno));
		return 4;
	}
	fread(&ahead,1,sizeof(ahead),anivfile);
	if ( feof(anivfile) )
	{
		free(verts);
		free(coords);
		free(faces);
		for ( int i=0; i<ngroup; i++ ) free(groups[i].faces);
		free(groups);
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	// check for Deus Ex's 16-bit vertex format
	int usedx;
	if ( (dhead.numverts*8) == ahead.framesize ) usedx = 1;
	else if ( (dhead.numverts*4) == ahead.framesize ) usedx = 0;
	else
	{
		free(verts);
		free(coords);
		free(faces);
		for ( int i=0; i<ngroup; i++ ) free(groups[i].faces);
		free(groups);
		fprintf(stderr," (Unknown format)\nIncorrect frame size,"
			" should be %u or %u. Wrong anivfile?\n",
			dhead.numverts*4,dhead.numverts*8);
		fclose(anivfile);
		return 16;
	}
	for ( int i=0; i<ahead.numframes; i++ )
	{
		// populate vertex list
		for ( int j=0; j<dhead.numverts; j++ )
		{
			if ( usedx ) fread(dxvert,4,sizeof(int16_t),anivfile);
			else fread(&avert,1,sizeof(avert),anivfile);
			if ( feof(anivfile) )
			{
				free(verts);
				free(coords);
				free(faces);
				for ( int k=0; k<ngroup; k++ )
					free(groups[i].faces);
				free(groups);
				fprintf(stderr,"Premature end of file reached "
					"at %lu\n",ftell(anivfile));
				fclose(anivfile);
				return 8;
			}
			if ( usedx )
			{
				verts[j].x = dxvert[0]/32768.f;
				verts[j].y = dxvert[1]/32768.f;
				verts[j].z = dxvert[2]/32768.f;
			}
			else
			{
				verts[j].x = unpackuvert(avert,0)/32768.f;
				verts[j].y = unpackuvert(avert,1)/32768.f;
				verts[j].z = unpackuvert(avert,2)/32768.f;
			}
		}
		// generate .obj file
		char fname[256] = {0};
		snprintf(fname,256,"%s_%d.obj",argv[3],i);
		FILE *obj = fopen(fname,"w");
		for ( int j=0; j<nvert; j++ )
			fprintf(obj,"v %f %f %f\n",verts[j].x,verts[j].y,
				verts[j].z);
		for ( int j=0; j<ncoord; j++ )
			fprintf(obj,"vt %f %f\n",coords[j].u,coords[j].v);
		for ( int j=0; j<ngroup; j++ )
		{
			fprintf(obj,"g SKIN%d\n",groups[j].mat);
			for ( int k=0; k<groups[j].nface; k++ )
			{
				int c = groups[j].faces[k];
				fprintf(obj," f %d/%d %d/%d %d/%d\n",
					faces[c].v[0]+1,faces[c].c[0]+1,
					faces[c].v[1]+1,faces[c].c[1]+1,
					faces[c].v[2]+1,faces[c].c[2]+1);
			}
		}
		fclose(obj);
	}
	free(verts);
	free(coords);
	free(faces);
	for ( int i=0; i<ngroup; i++ ) free(groups[i].faces);
	free(groups);
	return 0;
}

