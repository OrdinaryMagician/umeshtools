#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>

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
	int unmirror;
	int8_t pitch, yaw, roll;
	float scaleX, scaleY, scaleZ;
} transform_t;

// transform for base mesh
transform_t basetform =
{
	0.f, 0.f, 0.f,
	0,
	0, 0, 0,
	1.f, 1.f, 1.f
};
// transform for attachment mesh
transform_t attachtform =
{
	0.f, 0.f, 0.f,
	0,
	0, 0, 0,
	1.f, 1.f, 1.f
};
// additional scaling on output (useful to prevent wraparound)
float scaleOutX = 1.f, scaleOutY = 1.f, scaleOutZ = 1.f;

typedef struct
{
	float x, y, z;
} vector_t;

void normalize( vector_t *v )
{
	float scl = sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);
	v->x /= scl;
	v->y /= scl;
	v->z /= scl;
}

void transform( vector_t *v, transform_t *t )
{
	float pitch = (t->pitch/256.f)*2.f*acosf(-1.f),
		yaw = (t->yaw/256.f)*2.f*acosf(-1.f),
		roll = (t->roll/256.f)*2.f*acosf(-1.f);
	float s, c;
	vector_t v1, v2, v3, v4;
	// translation
	if ( t->unmirror )
	{
		roll *= -1;
		v->x *= -1.f;
		v->x += t->ofsX;
	}
	else v->x -= t->ofsX;
	v->y -= t->ofsY;
	v->z -= t->ofsZ;
	// scale
	v->x *= t->scaleX;
	v->y *= t->scaleY;
	v->z *= t->scaleZ;
	// rotation
	v1.x = v->x;
	v1.y = v->y;
	v1.z = v->z;
	// roll (x)
	s = sinf(roll);
	c = cosf(roll);
	v2.x = v1.x;
	v2.y = v1.y*c-v1.z*s;
	v2.z = v1.y*s+v1.z*c;
	// pitch (y)
	s = sinf(pitch);
	c = cosf(pitch);
	v3.x = v2.x*c+v2.z*s;
	v3.y = v2.y;
	v3.z = -v2.x*s+v2.z*c;
	// yaw (z)
	s = sinf(yaw);
	c = cosf(yaw);
	v4.x = v3.x*c-v3.y*s;
	v4.y = v3.x*s+v3.y*c;
	v4.z = v3.z;
	v->x = v4.x;
	v->y = v4.y;
	v->z = v4.z;
}

void detransform( vector_t *v, transform_t *t )
{
	// rotation
	float pitch = -(t->pitch/256.f)*2.f*acosf(-1.f),
		yaw = -(t->yaw/256.f)*2.f*acosf(-1.f),
		roll = -(t->roll/256.f)*2.f*acosf(-1.f);
	if ( t->unmirror ) roll *= -1;
	float s, c;
	vector_t v1, v2, v3, v4;
	v1.x = v->x;
	v1.y = v->y;
	v1.z = v->z;
	// yaw (z)
	s = sinf(yaw);
	c = cosf(yaw);
	v2.x = v1.x*c-v1.y*s;
	v2.y = v1.x*s+v1.y*c;
	v2.z = v1.z;
	// pitch (y)
	s = sinf(pitch);
	c = cosf(pitch);
	v3.x = v2.x*c+v2.z*s;
	v3.y = v2.y;
	v3.z = -v2.x*s+v2.z*c;
	// roll (x)
	s = sinf(roll);
	c = cosf(roll);
	v4.x = v3.x;
	v4.y = v3.y*c-v3.z*s;
	v4.z = v3.y*s+v3.z*c;
	v->x = v4.x;
	v->y = v4.y;
	v->z = v4.z;
	// scale
	v->x /= t->scaleX;
	v->y /= t->scaleY;
	v->z /= t->scaleZ;
	// translation
	if ( t->unmirror )
	{
		v->x -= t->ofsX;
		v->x *= -1.f;
	}
	else v->x += t->ofsX;
	v->y += t->ofsY;
	v->z += t->ofsZ;
	// extra scale
	v->x *= scaleOutX;
	v->y *= scaleOutY;
	v->z *= scaleOutZ;
}

vector_t *basev;	// vertices of base mesh
vector_t tri_pos, tri_X, tri_Y, tri_Z;	// computed weapon triangle coords
vector_t *attachv;	// vertices of attached mesh

// transforms a vector by the current weapon triangle coords
void transformbywtri( vector_t *v )
{
	vector_t vo = {tri_pos.x,tri_pos.y,tri_pos.z};
	vo.x += v->x*tri_X.x;
	vo.y += v->x*tri_X.y;
	vo.z += v->x*tri_X.z;
	vo.x += v->y*tri_Y.x;
	vo.y += v->y*tri_Y.y;
	vo.z += v->y*tri_Y.z;
	vo.x += v->z*tri_Z.x;
	vo.y += v->z*tri_Z.y;
	vo.z += v->z*tri_Z.z;
	v->x = vo.x;
	v->y = vo.y;
	v->z = vo.z;
}

// loaded data of base model
dataheader_t dhead_base;
datapoly_t *dpoly_base;
aniheader_t ahead_base;
uint32_t *avert_base;
dxvert_t *dxvert_base;

// loaded data of attached model
dataheader_t dhead_attach;
datapoly_t *dpoly_attach;
aniheader_t ahead_attach;
uint32_t *avert_attach;
dxvert_t *dxvert_attach;

// data to save of final model
dataheader_t dhead_out;
datapoly_t *dpoly_out;
aniheader_t ahead_out;
uint32_t *avert_out;
dxvert_t *dxvert_out;

int attachskinofs;	// multiskin offset for attached mesh in final file
int tri_index;		// index in base model for weapon triangle

int attachonly = 0;		// final model will only contain attached mesh

int main( int argc, char **argv )
{
	if ( argc < 7 )
	{
		fprintf(stderr,"usage: attacher <_a.3d base> <_d.3d base> <_a.3d attach> <_d.3d attach> <_a.3d output> <_d.3d output> [-p1 posX posY posZ] [-p2 posX posY posZ] [-f1] [-f2] [-r1 pitch yaw roll] [-r2 pitch yaw roll] [-s1 scaleX scaleY scaleZ] [-s2 scaleX scaleY scaleZ] [-s3 scaleX scaleY scaleZ] [-attachonly]\n");
		return 1;
	}
	FILE *datafile, *anivfile;
	// read base datafile
	if ( !(datafile = fopen(argv[2],"rb")) )
	{
		fprintf(stderr,"Cannot open datafile '%s': %s\n",argv[2],strerror(errno));
		return 2;
	}
	fread(&dhead_base,1,sizeof(dataheader_t),datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[2],ftell(datafile));
		fclose(datafile);
		return 8;
	}
	dpoly_base = calloc(dhead_base.numpolys,sizeof(datapoly_t));
	attachskinofs = 0;
	tri_index = -1;
	for ( int i=0; i<dhead_base.numpolys; i++ )
	{
		fread(&dpoly_base[i],1,sizeof(datapoly_t),datafile);
		if ( feof(datafile) )
		{
			free(dpoly_base);
			fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[2],ftell(datafile));
			fclose(datafile);
			return 8;
		}
		// get skin offset
		if ( dpoly_base[i].texnum >= attachskinofs ) attachskinofs++;
		// get triangle
		if ( dpoly_base[i].type&8 ) tri_index = i;
	}
	fclose(datafile);
	if ( tri_index == -1 )
	{
		free(dpoly_base);
		fprintf(stderr,"Datafile '%s' contains no weapon triangle\n",argv[2]);
		return 16;
	}
	// read base anivfile
	if ( !(anivfile = fopen(argv[1],"rb")) )
	{
		free(dpoly_base);
		fprintf(stderr,"Cannot open anivfile '%s': %s\n",argv[1],strerror(errno));
		return 2;
	}
	fread(&ahead_base,1,sizeof(aniheader_t),anivfile);
	if ( feof(anivfile) )
	{
		free(dpoly_base);
		fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[1],ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	// check for Deus Ex's 16-bit vertex format
	int usedx;
	if ( (dhead_base.numverts*8) == ahead_base.framesize )
	{
		usedx = 1;
		avert_base = 0;
		dxvert_base = calloc(dhead_base.numverts*ahead_base.numframes,sizeof(dxvert_t));
	}
	else if ( (dhead_base.numverts*4) == ahead_base.framesize )
	{
		usedx = 0;
		avert_base = calloc(dhead_base.numverts*ahead_base.numframes,sizeof(uint32_t));
		dxvert_base = 0;
	}
	else
	{
		free(dpoly_base);
		fprintf(stderr,"Incorrect frame size %u in '%s', should be %u or %u. Wrong anivfile?\n",ahead_base.framesize,argv[1],dhead_base.numverts*4,dhead_base.numverts*8);
		fclose(anivfile);
		return 16;
	}
	for ( int i=0; i<(dhead_base.numverts*ahead_base.numframes); i++ )
	{
		if ( usedx ) fread(&dxvert_base[i],1,sizeof(dxvert_t),anivfile);
		else fread(&avert_base[i],1,sizeof(uint32_t),anivfile);
		if ( feof(anivfile) )
		{
			free(dpoly_base);
			if ( dxvert_base ) free(dxvert_base);
			else free(avert_base);
			fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[1],ftell(anivfile));
			fclose(anivfile);
			return 8;
		}
	}
	fclose(anivfile);
	// read attachment datafile
	if ( !(datafile = fopen(argv[4],"rb")) )
	{
		free(dpoly_base);
		if ( dxvert_base ) free(dxvert_base);
		else free(avert_base);
		fprintf(stderr,"Cannot open datafile '%s': %s\n",argv[4],strerror(errno));
		return 2;
	}
	fread(&dhead_attach,1,sizeof(dataheader_t),datafile);
	if ( feof(datafile) )
	{
		free(dpoly_base);
		if ( dxvert_base ) free(dxvert_base);
		else free(avert_base);
		fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[4],ftell(datafile));
		fclose(datafile);
		return 8;
	}
	dpoly_attach = calloc(dhead_attach.numpolys,sizeof(datapoly_t));
	for ( int i=0; i<dhead_attach.numpolys; i++ )
	{
		fread(&dpoly_attach[i],1,sizeof(datapoly_t),datafile);
		if ( feof(datafile) )
		{
			free(dpoly_base);
			if ( dxvert_base ) free(dxvert_base);
			else free(avert_base);
			free(dpoly_attach);
			fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[4],ftell(datafile));
			fclose(datafile);
			return 8;
		}
	}
	fclose(datafile);
	// read attachment anivfile
	if ( !(anivfile = fopen(argv[3],"rb")) )
	{
		free(dpoly_base);
		if ( dxvert_base ) free(dxvert_base);
		else free(avert_base);
		free(dpoly_attach);
		fprintf(stderr,"Cannot open anivfile '%s': %s\n",argv[3],strerror(errno));
		return 2;
	}
	fread(&ahead_attach,1,sizeof(aniheader_t),anivfile);
	if ( feof(anivfile) )
	{
		free(dpoly_base);
		if ( dxvert_base ) free(dxvert_base);
		else free(avert_base);
		free(dpoly_attach);
		fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[3],ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	// check for Deus Ex's 16-bit vertex format
	if ( (dhead_attach.numverts*8) == ahead_attach.framesize )
	{
		usedx = 1;
		avert_attach = 0;
		dxvert_attach = calloc(dhead_attach.numverts*ahead_base.numframes,sizeof(dxvert_t));
	}
	else if ( (dhead_attach.numverts*4) == ahead_attach.framesize )
	{
		usedx = 0;
		avert_attach = calloc(dhead_attach.numverts*ahead_base.numframes,sizeof(uint32_t));
		dxvert_attach = 0;
	}
	else
	{
		free(dpoly_base);
		if ( dxvert_base ) free(dxvert_base);
		else free(avert_base);
		free(dpoly_attach);
		fprintf(stderr,"Incorrect frame size %u in '%s', should be %u or %u. Wrong anivfile?\n",ahead_attach.framesize,argv[3],dhead_attach.numverts*4,dhead_attach.numverts*8);
		fclose(anivfile);
		return 16;
	}
	// read base frame
	for ( int i=0; i<dhead_attach.numverts; i++ )
	{
		if ( usedx ) fread(&dxvert_attach[i],1,sizeof(dxvert_t),anivfile);
		else fread(&avert_attach[i],1,sizeof(uint32_t),anivfile);
		if ( feof(anivfile) )
		{
			free(dpoly_base);
			if ( dxvert_base ) free(dxvert_base);
			else free(avert_base);
			free(dpoly_attach);
			if ( dxvert_attach ) free(dxvert_attach);
			else free(avert_attach);
			fprintf(stderr,"Premature end of file reached at '%s':%lu\n",argv[3],ftell(anivfile));
			fclose(anivfile);
			return 8;
		}
	}
	fclose(anivfile);
	// duplicate the vertices for as many frames as the base model has
	for ( int i=1; i<ahead_base.numframes; i++ )
	{
		if ( dxvert_attach ) memcpy(dxvert_attach+i*dhead_attach.numverts,dxvert_attach,dhead_attach.numverts*sizeof(dxvert_t));
		else memcpy(avert_attach+i*dhead_attach.numverts,avert_attach,dhead_attach.numverts*sizeof(uint32_t));
	}
	// read transforms
	for ( int i=5; i<argc; i++ )
	{
		if ( !strcmp(argv[i],"-p1") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%f",&basetform.ofsX);
			sscanf(argv[++i],"%f",&basetform.ofsY);
			sscanf(argv[++i],"%f",&basetform.ofsZ);
		}
		else if ( !strcmp(argv[i],"-p2") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%f",&attachtform.ofsX);
			sscanf(argv[++i],"%f",&attachtform.ofsY);
			sscanf(argv[++i],"%f",&attachtform.ofsZ);
		}
		else if ( !strcmp(argv[i],"-f1") ) basetform.unmirror = 1;
		else if ( !strcmp(argv[i],"-f2") ) attachtform.unmirror = 1;
		else if ( !strcmp(argv[i],"-r1") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%hhd",&basetform.pitch);
			sscanf(argv[++i],"%hhd",&basetform.yaw);
			sscanf(argv[++i],"%hhd",&basetform.roll);
		}
		else if ( !strcmp(argv[i],"-r2") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%hhd",&attachtform.pitch);
			sscanf(argv[++i],"%hhd",&attachtform.yaw);
			sscanf(argv[++i],"%hhd",&attachtform.roll);
		}
		else if ( !strcmp(argv[i],"-s1") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%f",&basetform.scaleX);
			sscanf(argv[++i],"%f",&basetform.scaleY);
			sscanf(argv[++i],"%f",&basetform.scaleZ);
		}
		else if ( !strcmp(argv[i],"-s2") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%f",&attachtform.scaleX);
			sscanf(argv[++i],"%f",&attachtform.scaleY);
			sscanf(argv[++i],"%f",&attachtform.scaleZ);
		}
		else if ( !strcmp(argv[i],"-s3") )
		{
			if ( argc < i+3 ) break;
			sscanf(argv[++i],"%f",&scaleOutX);
			sscanf(argv[++i],"%f",&scaleOutY);
			sscanf(argv[++i],"%f",&scaleOutZ);
		}
		else if ( !strcmp(argv[i],"-attachonly") ) attachonly = 1;
	}
	// change triangle winding when mirroring
	if ( basetform.unmirror )
	{
		for ( int i=0; i<dhead_base.numpolys; i++ )
		{
			int tmp = dpoly_base[i].vertices[1];
			dpoly_base[i].vertices[1] = dpoly_base[i].vertices[2];
			dpoly_base[i].vertices[2] = tmp;
			tmp = dpoly_base[i].uv[1][0];
			dpoly_base[i].uv[1][0] = dpoly_base[i].uv[2][0];
			dpoly_base[i].uv[2][0] = tmp;
			tmp = dpoly_base[i].uv[1][1];
			dpoly_base[i].uv[1][1] = dpoly_base[i].uv[2][1];
			dpoly_base[i].uv[2][1] = tmp;
		}
	}
	if ( attachtform.unmirror )
	{
		for ( int i=0; i<dhead_attach.numpolys; i++ )
		{
			int tmp = dpoly_attach[i].vertices[1];
			dpoly_attach[i].vertices[1] = dpoly_attach[i].vertices[2];
			dpoly_attach[i].vertices[2] = tmp;
			tmp = dpoly_attach[i].uv[1][0];
			dpoly_attach[i].uv[1][0] = dpoly_attach[i].uv[2][0];
			dpoly_attach[i].uv[2][0] = tmp;
			tmp = dpoly_attach[i].uv[1][1];
			dpoly_attach[i].uv[1][1] = dpoly_attach[i].uv[2][1];
			dpoly_attach[i].uv[2][1] = tmp;
		}
	}
	// transform base vertices
	basev = calloc(dhead_base.numverts*ahead_base.numframes,sizeof(vector_t));
	for ( int i=0; i<(dhead_base.numverts*ahead_base.numframes); i++ )
	{
		if ( dxvert_base )
		{
			basev[i].x = dxvert_base[i].x;
			basev[i].y = dxvert_base[i].y;
			basev[i].z = dxvert_base[i].z;
		}
		else
		{
			basev[i].x = unpackuvert(avert_base[i],0)/32.f;
			basev[i].y = unpackuvert(avert_base[i],1)/32.f;
			basev[i].z = unpackuvert(avert_base[i],2)/64.f;
		}
		transform(&basev[i],&basetform);
	}
	// transform attachment vertices (pass 1)
	attachv = calloc(dhead_attach.numverts*ahead_base.numframes,sizeof(vector_t));
	for ( int i=0; i<(dhead_attach.numverts*ahead_base.numframes); i++ )
	{
		if ( dxvert_attach )
		{
			attachv[i].x = dxvert_attach[i].x;
			attachv[i].y = dxvert_attach[i].y;
			attachv[i].z = dxvert_attach[i].z;
		}
		else
		{
			attachv[i].x = unpackuvert(avert_attach[i],0)/32.f;
			attachv[i].y = unpackuvert(avert_attach[i],1)/32.f;
			attachv[i].z = unpackuvert(avert_attach[i],2)/64.f;
		}
		transform(&attachv[i],&attachtform);
	}
	// transform attachment vertices (pass 2)
	for ( int i=0; i<ahead_base.numframes; i++ )
	{
		int va = dpoly_base[tri_index].vertices[0]+i*dhead_base.numverts;
		int vb = dpoly_base[tri_index].vertices[1]+i*dhead_base.numverts;
		int vc = dpoly_base[tri_index].vertices[2]+i*dhead_base.numverts;
		// origin is midpoint between vertices 0 and 1
		tri_pos.x = (basev[va].x+basev[vb].x)/2.;
		tri_pos.y = (basev[va].y+basev[vb].y)/2.;
		tri_pos.z = (basev[va].z+basev[vb].z)/2.;
		// Z is normalized vector from 0 to 1
		tri_Z.x = basev[va].x-basev[vb].x;
		tri_Z.y = basev[va].y-basev[vb].y;
		tri_Z.z = basev[va].z-basev[vb].z;
		normalize(&tri_Z);
		// Y is normal of triangle
		vector_t ac, ab;
		ac.x = basev[vc].x-basev[va].x;
		ac.y = basev[vc].y-basev[va].y;
		ac.z = basev[vc].z-basev[va].z;
		ab.x = basev[vb].x-basev[va].x;
		ab.y = basev[vb].y-basev[va].y;
		ab.z = basev[vb].z-basev[va].z;
		normalize(&ac);
		normalize(&ab);
		tri_Y.x = ac.y*ab.z-ac.z*ab.y;
		tri_Y.y = ac.z*ab.x-ac.x*ab.z;
		tri_Y.z = ac.x*ab.y-ac.y*ab.x;
		// X is cross product of both
		tri_X.x = tri_Y.y*tri_Z.z-tri_Y.z*tri_Z.y;
		tri_X.y = tri_Y.z*tri_Z.x-tri_Y.x*tri_Z.z;
		tri_X.z = tri_Y.x*tri_Z.y-tri_Y.y*tri_Z.x;
		for ( int j=0; j<dhead_attach.numverts; j++ )
			transformbywtri(&attachv[j+i*dhead_attach.numverts]);
	}
	// detransform all vertices for final model
	if ( basetform.unmirror )
	{
		for ( int i=0; i<dhead_base.numpolys; i++ )
		{
			int tmp = dpoly_base[i].vertices[1];
			dpoly_base[i].vertices[1] = dpoly_base[i].vertices[2];
			dpoly_base[i].vertices[2] = tmp;
			tmp = dpoly_base[i].uv[1][0];
			dpoly_base[i].uv[1][0] = dpoly_base[i].uv[2][0];
			dpoly_base[i].uv[2][0] = tmp;
			tmp = dpoly_base[i].uv[1][1];
			dpoly_base[i].uv[1][1] = dpoly_base[i].uv[2][1];
			dpoly_base[i].uv[2][1] = tmp;
		}
		for ( int i=0; i<dhead_attach.numpolys; i++ )
		{
			int tmp = dpoly_attach[i].vertices[1];
			dpoly_attach[i].vertices[1] = dpoly_attach[i].vertices[2];
			dpoly_attach[i].vertices[2] = tmp;
			tmp = dpoly_attach[i].uv[1][0];
			dpoly_attach[i].uv[1][0] = dpoly_attach[i].uv[2][0];
			dpoly_attach[i].uv[2][0] = tmp;
			tmp = dpoly_attach[i].uv[1][1];
			dpoly_attach[i].uv[1][1] = dpoly_attach[i].uv[2][1];
			dpoly_attach[i].uv[2][1] = tmp;
		}
	}
	for ( int i=0; i<(dhead_base.numverts*ahead_base.numframes); i++ )
	{
		detransform(&basev[i],&basetform);
		if ( dxvert_base )
		{
			dxvert_base[i].x = basev[i].x;
			dxvert_base[i].y = basev[i].y;
			dxvert_base[i].z = basev[i].z;
		}
		else avert_base[i] = packuvert(basev[i].x*32.f,basev[i].y*32.f,basev[i].z*64.f);
	}
	free(basev);
	for ( int i=0; i<(dhead_attach.numverts*ahead_base.numframes); i++ )
	{
		detransform(&attachv[i],&basetform);
		if ( dxvert_base && !dxvert_attach )
		{
			free(avert_attach);
			avert_attach = 0;
			dxvert_attach = calloc(dhead_attach.numverts*ahead_base.numframes,sizeof(dxvert_t));
		}
		else if ( avert_base && !avert_attach )
		{
			free(dxvert_attach);
			dxvert_attach = 0;
			avert_attach = calloc(dhead_attach.numverts*ahead_base.numframes,sizeof(uint32_t));
		}
		if ( dxvert_base )
		{
			dxvert_attach[i].x = attachv[i].x;
			dxvert_attach[i].y = attachv[i].y;
			dxvert_attach[i].z = attachv[i].z;
		}
		else avert_attach[i] = packuvert(attachv[i].x*32.f,attachv[i].y*32.,attachv[i].z*64.f);
	}
	free(attachv);
	// compose final model files
	memset(&dhead_out,0,sizeof(dataheader_t));
	if ( attachonly )
	{
		dhead_out.numpolys = dhead_attach.numpolys;
		dhead_out.numverts = dhead_attach.numverts;
	}
	else
	{
		dhead_out.numpolys = dhead_base.numpolys+dhead_attach.numpolys-1;	// exclude weapon triangle
		dhead_out.numverts = dhead_base.numverts+dhead_attach.numverts;
	}
	dpoly_out = calloc(dhead_out.numpolys,sizeof(datapoly_t));
	int j = 0;
	if ( !attachonly )
	{
		for ( int i=0; i<dhead_base.numpolys; i++ )
		{
			if ( dpoly_base[i].type&8 ) continue;
			memcpy(dpoly_out+j,dpoly_base+i,sizeof(datapoly_t));
			j++;
		}
	}
	for ( int i=0; i<dhead_attach.numpolys; i++ )
	{
		memcpy(dpoly_out+j,dpoly_attach+i,sizeof(datapoly_t));
		if ( !attachonly )
		{
			dpoly_out[j].vertices[0] += dhead_base.numverts;
			dpoly_out[j].vertices[1] += dhead_base.numverts;
			dpoly_out[j].vertices[2] += dhead_base.numverts;
			dpoly_out[j].texnum += attachskinofs;
		}
		j++;
	}
	memset(&ahead_out,0,sizeof(aniheader_t));
	ahead_out.numframes = ahead_base.numframes;
	if ( dxvert_base )
	{
		ahead_out.framesize = dhead_out.numverts*8;
		dxvert_out = calloc(dhead_out.numverts*ahead_out.numframes,sizeof(dxvert_t));
		for ( int i=0; i<ahead_out.numframes; i++ )
		{
			int k = 0;
			if ( !attachonly )
			{
				for ( int j=0; j<dhead_base.numverts; j++ )
				{
					dxvert_out[k+i*dhead_out.numverts].x = dxvert_base[j+i*dhead_base.numverts].x;
					dxvert_out[k+i*dhead_out.numverts].y = dxvert_base[j+i*dhead_base.numverts].y;
					dxvert_out[k+i*dhead_out.numverts].z = dxvert_base[j+i*dhead_base.numverts].z;
					k++;
				}
			}
			for ( int j=0; j<dhead_attach.numverts; j++ )
			{
				dxvert_out[k+i*dhead_out.numverts].x = dxvert_attach[j+i*dhead_attach.numverts].x;
				dxvert_out[k+i*dhead_out.numverts].y = dxvert_attach[j+i*dhead_attach.numverts].y;
				dxvert_out[k+i*dhead_out.numverts].z = dxvert_attach[j+i*dhead_attach.numverts].z;
				k++;
			}
		}
	}
	else
	{
		ahead_out.framesize = dhead_out.numverts*4;
		avert_out = calloc(dhead_out.numverts*ahead_out.numframes,sizeof(uint32_t));
		for ( int i=0; i<ahead_out.numframes; i++ )
		{
			int k = 0;
			if ( !attachonly )
			{
				for ( int j=0; j<dhead_base.numverts; j++ )
				{
					avert_out[k+i*dhead_out.numverts] = avert_base[j+i*dhead_base.numverts];
					k++;
				}
			}
			for ( int j=0; j<dhead_attach.numverts; j++ )
			{
				avert_out[k+i*dhead_out.numverts] = avert_attach[j+i*dhead_attach.numverts];
				k++;
			}
		}
	}
	free(dpoly_base);
	if ( dxvert_base ) free(dxvert_base);
	else free(avert_base);
	free(dpoly_attach);
	if ( dxvert_attach ) free(dxvert_attach);
	else free(avert_attach);
	// write final model files
	if ( !(datafile = fopen(argv[6],"wb")) )
	{
		free(dpoly_out);
		if ( dxvert_out ) free(dxvert_out);
		else free(avert_out);
		fprintf(stderr,"Cannot open datafile '%s': %s\n",argv[6],strerror(errno));
		return 4;
	}
	fwrite(&dhead_out,1,sizeof(dataheader_t),datafile);
	fwrite(dpoly_out,dhead_out.numpolys,sizeof(datapoly_t),datafile);
	fclose(datafile);
	if ( !(anivfile = fopen(argv[5],"wb")) )
	{
		free(dpoly_out);
		if ( dxvert_out ) free(dxvert_out);
		else free(avert_out);
		fprintf(stderr,"Cannot open anivfile '%s': %s\n",argv[5],strerror(errno));
		return 4;
	}
	fwrite(&ahead_out,1,sizeof(aniheader_t),anivfile);
	if ( dxvert_out ) fwrite(dxvert_out,1,ahead_out.numframes*ahead_out.framesize,anivfile);
	else fwrite(avert_out,1,ahead_out.numframes*ahead_out.framesize,anivfile);
	fclose(anivfile);
	free(dpoly_out);
	if ( dxvert_out ) free(dxvert_out);
	else free(avert_out);
	return 0;
}
