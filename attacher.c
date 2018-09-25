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

// transforms for the attached weapon
typedef struct
{
	float ofsX, ofsY, ofsZ;
	int flipX, flipY, flipZ;
	uint8_t pitch, yaw, roll;
	float scaleX, scaleY, scaleZ;
} transform_t;

transform_t basetform;		// transform for base mesh
transform_t attachtform;	// transform for attachment mesh

typedef struct
{
	float x, y, z;
} vector_t;

vector_t *basev;	// vertices of base mesh
vector_t tri_pos, tri_X, tri_Y, tri_Z;	// computed weapon triangle vectors
vector_t *attachv;	// vertices of attached mesh

dataheader_t dhead_base;
datapoly_t *dpoly_base;
aniheader_t *ahead_base;
uint32_t *avert_base;
dxvert_t *dxvert_base;

dataheader_t dhead_attach;
datapoly_t *dpoly_attach;
aniheader_t *ahead_attach;
uint32_t *avert_attach;
dxvert_t *dxvert_attach;

dataheader_t dhead_out;
datapoly_t *dpoly_out;
aniheader_t *ahead_out;
uint32_t *avert_out;
dxvert_t *dxvert_out;

int main( int argc, char **argv )
{
	(void)argv;
	if ( argc < 7 )
	{
		fprintf(stderr,"usage: attacher <_a.3d base> <_d.3d base>"
			" <_a.3d attach> <_d.3d attach> <_a.3d output> <_d.3d"
			" output> [-p1 posX posY posZ] [-p2 posX posY posZ]"
			" [-f1 flipX flipY flipZ] [-f2 flipX flipY flipZ]"
			" [-r1 pitch yaw roll] [-r2 pitch yaw roll] [-s1"
			" scaleX scaleY scaleZ] [-s2 scaleX scaleY scaleZ]\n");
		return 1;
	}
	return 0;
}
