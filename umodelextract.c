#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define UPKG_MAGIC 0x9E2A83C1

// uncomment if you want full data dumps, helpful if you need to reverse engineer some unsupported format
//#define _DEBUG 1

typedef struct
{
	uint32_t magic;
	uint16_t pkgver, license;
	uint32_t flags, nnames, onames, nexports, oexports, nimports, oimports;
} upkg_header_t;

uint8_t *pkgfile;
upkg_header_t *head;
size_t fpos = 0;

uint8_t readbyte( void )
{
	uint8_t val = pkgfile[fpos];
	fpos++;
	return val;
}

uint16_t readword( void )
{
	uint16_t val = *(uint16_t*)(pkgfile+fpos);
	fpos += 2;
	return val;
}

uint32_t readdword( void )
{
	uint32_t val = *(uint32_t*)(pkgfile+fpos);
	fpos += 4;
	return val;
}

float readfloat( void )
{
	float val = *(float*)(pkgfile+fpos);
	fpos += 4;
	return val;
}

#define READSTRUCT(x,y) {memcpy(&x,pkgfile+fpos,sizeof(y));fpos+=sizeof(y);}

// reads a compact index value
int32_t readindex( void )
{
	uint8_t byte[5] = {0};
	byte[0] = readbyte();
	if ( !byte[0] ) return 0;
	if ( byte[0]&0x40 )
	{
		for ( int i=1; i<5; i++ )
		{
			byte[i] = readbyte();
			if ( !(byte[i]&0x80) ) break;
		}
	}
	int32_t tf = byte[0]&0x3f;
	tf |= (int32_t)(byte[1]&0x7f)<<6;
	tf |= (int32_t)(byte[2]&0x7f)<<13;
	tf |= (int32_t)(byte[3]&0x7f)<<20;
	tf |= (int32_t)(byte[4]&0x7f)<<27;
	if ( byte[0]&0x80 ) tf *= -1;
	return tf;
}

// reads a name table entry
size_t readname( int *olen )
{
	size_t pos = fpos;
	if ( head->pkgver >= 64 )
	{
		int32_t len = readindex();
		pos = fpos;
		if ( olen ) *olen = len;
		if ( len <= 0 ) return pos;
		fpos += len;
	}
	else
	{
		int c, p = 0;
		while ( (c = readbyte()) ) p++;
		if ( olen ) *olen = p;
	}
	fpos += 4;
	return pos;
}

size_t getname( int index, int *olen )
{
	size_t prev = fpos;
	fpos = head->onames;
	size_t npos = 0;
	for ( int i=0; i<=index; i++ )
		npos = readname(olen);
	fpos = prev;
	return npos;
}

// checks if a name exists
int hasname( const char *needle )
{
	if ( !needle ) return 0;
	size_t prev = fpos;
	fpos = head->onames;
	int found = 0;
	int nlen = strlen(needle);
	for ( uint32_t i=0; i<head->nnames; i++ )
	{
		int32_t len = 0;
		if ( head->pkgver >= 64 )
		{
			len = readindex();
			if ( len <= 0 ) continue;
		}
		int c = 0, p = 0, match = 1;
		while ( (c = readbyte()) )
		{
			if ( (p >= nlen) || (needle[p] != c) ) match = 0;
			p++;
			if ( len && (p > len) ) break;
		}
		if ( match )
		{
			found = 1;
			break;
		}
		fpos += 4;
	}
	fpos = prev;
	return found;
}

// read import table entry and return index of its object name
int32_t readimport( void )
{
	readindex();
	readindex();
	if ( head->pkgver >= 55 ) fpos += 4;
	else readindex();
	return readindex();
}

int32_t getimport( int index )
{
	size_t prev = fpos;
	fpos = head->oimports;
	int32_t iname = 0;
	for ( int i=0; i<=index; i++ )
		iname = readimport();
	fpos = prev;
	return iname;
}

// fully read import table entry
void readimport2( int32_t *cpkg, int32_t *cname, int32_t *pkg, int32_t *name )
{
	*cpkg = readindex();
	*cname = readindex();
	if ( head->pkgver >= 55 ) *pkg = readdword();
	else *pkg = readindex();
	*name = readindex();
}

void getimport2( int index, int32_t *cpkg, int32_t *cname, int32_t *pkg,
	int32_t *name )
{
	size_t prev = fpos;
	fpos = head->oimports;
	for ( int i=0; i<=index; i++ )
		readimport2(cpkg,cname,pkg,name);
	fpos = prev;
}

// read export table entry
void readexport( int32_t *class, int32_t *ofs, int32_t *siz, int32_t *name )
{
	*class = readindex();
	readindex();
	if ( head->pkgver >= 55 ) fpos += 4;
	*name = readindex();
	fpos += 4;
	*siz = readindex();
	if ( *siz > 0 ) *ofs = readindex();
}

void getexport( int index, int32_t *class, int32_t *ofs, int32_t *siz,
	int32_t *name )
{
	size_t prev = fpos;
	fpos = head->oexports;
	for ( int i=0; i<=index; i++ )
		readexport(class,ofs,siz,name);
	fpos = prev;
}

// fully read export table entry
void readexport2( int32_t *class, int32_t *super, int32_t *pkg, int32_t *name,
	uint32_t *flags, int32_t *siz, int32_t *ofs )
{
	*class = readindex();
	*super = readindex();
	if ( head->pkgver >= 55 ) *pkg = readdword();
	else *pkg = 0;
	*name = readindex();
	*flags = readdword();
	*siz = readindex();
	if ( *siz > 0 ) *ofs = readindex();
}

void getexport2( int index, int32_t *class, int32_t *super, int32_t *pkg,
	int32_t *name, uint32_t *flags, int32_t *siz, int32_t *ofs )
{
	size_t prev = fpos;
	fpos = head->oexports;
	for ( int i=0; i<=index; i++ )
		readexport2(class,super,pkg,name,flags,siz,ofs);
	fpos = prev;
}

typedef struct
{
	float x, y, z;
} __attribute__((packed)) vector_t;

typedef struct
{
	int32_t pitch, yaw, roll;
} __attribute__((packed)) rotator_t;

typedef struct
{
	vector_t min, max;
	uint8_t isvalid;
} __attribute__((packed)) boundingbox_t;

typedef struct
{
	vector_t position;
} __attribute__((packed)) boundingsphere_61pre_t;

typedef struct
{
	vector_t position;
	float w;
} __attribute__((packed)) boundingsphere_t;

typedef struct
{
	int16_t x, y, z, pad;
} __attribute__((packed)) vert_dx_t;

typedef struct
{
	uint16_t verts[3];
	uint8_t uv[3][2];
	uint32_t flags, texnum;
} __attribute__((packed)) tri_t;

typedef struct
{
	float time;
	int32_t function;
} __attribute__((packed)) animfunction_t;

typedef struct
{
	int32_t name, group;
	uint32_t start_frame, num_frames, function_count;
	animfunction_t *functions;
	float rate;
} __attribute__((packed)) animseq_t;

typedef struct
{
	uint32_t numverttriangles, trianglelistoffset;
} __attribute__((packed)) connect_t;

typedef struct
{
	boundingbox_t boundingbox;
	boundingsphere_t boundingsphere;
	uint32_t verts_jump, verts_count;
	uint32_t *verts_ue1;
	vert_dx_t *verts_dx;
	uint32_t tris_jump, tris_count;
	tri_t *tris;
	uint32_t animseqs_count;
	animseq_t *animseqs;
	uint32_t connects_jump, connects_count;
	connect_t *connects;
	boundingbox_t boundingbox2;
	boundingsphere_t boundingsphere2;
	uint32_t vertlinks_jump, vertlinks_count;
	uint32_t *vertlinks;
	uint32_t textures_count;
	int32_t *textures;
	uint32_t boundingboxes_count;
	boundingbox_t *boundingboxes;
	uint32_t boundingspheres_count;
	boundingsphere_t *boundingspheres;
	uint32_t frameverts, animframes, andflags, orflags;
	vector_t scale, origin;
	rotator_t rotorigin;
	uint32_t curpoly, curvertex;
	uint32_t texturelod_count;
	float *texturelods;
} __attribute__((packed)) mesh_t;

typedef struct
{
	uint16_t wedges[3];
	uint16_t material;
} __attribute__((packed)) lodface_t;

typedef struct
{
	uint16_t vertex;
	uint8_t st[2];
} __attribute__((packed)) wedge_t;

typedef struct
{
	uint32_t flags, texnum;
} __attribute__((packed)) material_t;

typedef struct
{
	uint32_t collapsepointthus_count;
	uint16_t *collapsepointthus;
	uint32_t facelevel_count;
	uint16_t *facelevels;
	uint32_t faces_count;
	lodface_t *faces;
	uint32_t collapsewedgethus_count;
	uint16_t *collapsewedgethus;
	uint32_t wedges_count;
	wedge_t *wedges;
	uint32_t materials_count;
	material_t *materials;
	uint32_t specialfaces_count;
	lodface_t *specialfaces;
	uint32_t modelverts, specialverts;
	float meshscalemax, lodhysteresis, lodstrength;
	uint32_t lodminverts;
	float lodmorph, lodzdistance;
	uint32_t remapanimverts_count;
	uint16_t *remapanimverts;
	uint32_t oldframeverts;
} __attribute__((packed)) lodmesh_t;

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
	switch( c )
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

// mesh-relevant poly flags
#define PF_INVISIBLE   0x01
#define PF_MASKED      0x02
#define PF_TRANSLUCENT 0x04
#define PF_ENVIRONMENT 0x10
#define PF_MODULATED   0x40
#define PF_TWOSIDED    0x100
#define PF_NOSMOOTH    0x800
#define PF_SPECIALPOLY 0x1000
#define PF_FLAT        0x4000
#define PF_UNLIT       0x400000

uint8_t typefromflags( uint32_t flags )
{
	uint8_t out = 0;
	if ( flags&PF_MODULATED ) out = 4;
	else if ( flags&PF_MASKED ) out = 3;
	else if ( flags&PF_TRANSLUCENT ) out = 2;
	else if ( flags&PF_TWOSIDED ) out = 1;
	// old packages use PF_INVISIBLE for the weapon triangle
	if ( flags&(PF_INVISIBLE|PF_SPECIALPOLY) ) out |= 0x08;
	if ( flags&PF_UNLIT ) out |= 0x10;
	if ( flags&PF_FLAT ) out |= 0x20;
	if ( flags&PF_ENVIRONMENT ) out |= 0x40;
	if ( flags&PF_NOSMOOTH ) out |= 0x80;
	return out;
}

uint32_t flagsfromtype( uint8_t type )
{
	uint32_t out = 0;
	if ( (type&7) == 4 ) out |= PF_MODULATED;
	else if ( (type&7) == 3 ) out |= PF_MASKED;
	else if ( (type&7) == 2 ) out |= PF_TRANSLUCENT;
	else if ( (type&7) == 1 ) out |= PF_TWOSIDED;
	if ( type&0x08 ) out |= PF_SPECIALPOLY;
	if ( type&0x10 ) out |= PF_UNLIT;
	if ( type&0x20 ) out |= PF_FLAT;
	if ( type&0x40 ) out |= PF_ENVIRONMENT;
	if ( type&0x08 ) out |= PF_NOSMOOTH;
	return out;
}

// U1 beta packages actually use the same triangle format as the raw file
// so it has to be converted
void readbetatri( tri_t *out )
{
	datapoly_t dp;
	READSTRUCT(dp,datapoly_t)
	out->verts[0] = dp.vertices[0];
	out->verts[1] = dp.vertices[1];
	out->verts[2] = dp.vertices[2];
	out->uv[0][0] = dp.uv[0][0];
	out->uv[0][1] = dp.uv[0][1];
	out->uv[1][0] = dp.uv[1][0];
	out->uv[1][1] = dp.uv[1][1];
	out->uv[2][0] = dp.uv[2][0];
	out->uv[2][1] = dp.uv[2][1];
	out->flags = flagsfromtype(dp.type);
	out->texnum = dp.texnum;
}

// construct full name for object
// shamelessly recycled from my old upackage project
void imprefix( FILE *f, int32_t i );
void exprefix( FILE *f, int32_t i );
void imprefix( FILE *f, int32_t i )
{
	int32_t cpkg, cnam, pkg, nam;
	getimport2(i,&cpkg,&cnam,&pkg,&nam);
	if ( pkg < 0 ) imprefix(f,-pkg-1);
	else if ( pkg > 0 ) exprefix(f,pkg-1);
	if ( pkg ) fprintf(f,".");
	int32_t l;
	char *pname = (char*)(pkgfile+getname(nam,&l));
	fprintf(f,"%.*s",l,pname);
}
void exprefix( FILE *f, int32_t i )
{
	int32_t cls, sup, pkg, nam, siz, ofs;
	uint32_t fl;
	getexport2(i,&cls,&sup,&pkg,&nam,&fl,&siz,&ofs);
	if ( pkg > 0 )
	{
		exprefix(f,pkg-1);
		fprintf(f,".");
	}
	int32_t l;
	char *pname = (char*)(pkgfile+getname(nam,&l));
	fprintf(f,"%.*s",l,pname);
}
void construct_fullname( FILE *f, int32_t i )
{
	if ( i > 0 ) exprefix(f,i-1);
	else if ( i < 0 ) imprefix(f,-i-1);
	else fprintf(f,"None");
}

// let the horrendous clusterfuck begin
void savemodel( int32_t namelen, char *name, int islodmesh, int version )
{
	char fname[256] = {0};
	size_t prev = fpos;
	FILE *f;
	int isdeusex;
	mesh_t mesh;
	lodmesh_t lodmesh;
	// shut the compiler up
	memset(&mesh,0,sizeof(mesh_t));
	memset(&lodmesh,0,sizeof(lodmesh_t));
	READSTRUCT(mesh.boundingbox,boundingbox_t)
	if ( version > 61 ) READSTRUCT(mesh.boundingsphere,boundingsphere_t)
	else if ( version < 40 ) fpos += 16;
	else READSTRUCT(mesh.boundingsphere,boundingsphere_61pre_t)
	if ( version > 61 ) mesh.verts_jump = readdword();
	mesh.verts_count = readindex();
	if ( version <= 61 ) mesh.verts_jump = fpos+mesh.verts_count*4;
	// check for deus ex format
	// corner case: mesh with zero vertices (thanks, klingon honor guard)
	if ( (mesh.verts_count == 0)
		|| (mesh.verts_jump-fpos)/mesh.verts_count == 4 )
	{
		isdeusex = 0;
		printf(" DETECTED STANDARD FORMAT\n");
	}
	else if ( (mesh.verts_jump-fpos)/mesh.verts_count == 8 )
	{
		isdeusex = 1;
		printf(" DETECTED DEUS EX FORMAT\n");
	}
	else
	{
		printf(" UNKNOWN VERTEX FORMAT, MESH IS CORRUPTED\n");
		return;
	}
	if ( isdeusex )
	{
		mesh.verts_dx = calloc(mesh.verts_count,8);
		memcpy(mesh.verts_dx,pkgfile+fpos,mesh.verts_count*8);
	}
	else
	{
		mesh.verts_ue1 = calloc(mesh.verts_count,4);
		memcpy(mesh.verts_ue1,pkgfile+fpos,mesh.verts_count*4);
	}
	fpos = mesh.verts_jump;
	if ( version > 61 ) mesh.tris_jump = readdword();
	mesh.tris_count = readindex();
	if ( version < 40 )
		mesh.tris_jump = fpos+mesh.tris_count*sizeof(datapoly_t);
	else if ( version <= 61 )
		mesh.tris_jump = fpos+mesh.tris_count*sizeof(tri_t);
	mesh.tris = calloc(mesh.tris_count,sizeof(tri_t));
	if ( version < 40 )
	{
		for ( uint32_t i=0; i<mesh.tris_count; i++ )
			readbetatri(&mesh.tris[i]);
	}
	else memcpy(mesh.tris,pkgfile+fpos,mesh.tris_count*sizeof(tri_t));
	fpos = mesh.tris_jump;
	mesh.animseqs_count = readindex();
	// this one can't be memcpy'd since there are index types
	mesh.animseqs = calloc(mesh.animseqs_count,sizeof(animseq_t));
	for ( uint32_t i=0; i<mesh.animseqs_count; i++ )
	{
		mesh.animseqs[i].name = readindex();
		if ( version >= 40 ) mesh.animseqs[i].group = readindex();
		if ( version < 40 )
		{
			mesh.animseqs[i].start_frame = readword();
			mesh.animseqs[i].num_frames = readword();
			fpos += 4;	// unidentified data
		}
		else
		{
			mesh.animseqs[i].start_frame = readdword();
			mesh.animseqs[i].num_frames = readdword();
			mesh.animseqs[i].function_count = readindex();
			mesh.animseqs[i].functions =
				calloc(mesh.animseqs[i].function_count,
				sizeof(animfunction_t));
			for ( uint32_t j=0; j<mesh.animseqs[i].function_count;
				j++ )
			{
				mesh.animseqs[i].functions[j].time =
					readfloat();
				mesh.animseqs[i].functions[j].function =
					readindex();
			}
		}
		mesh.animseqs[i].rate = readfloat();
	}
	if ( version > 61 ) mesh.connects_jump = readdword();
	mesh.connects_count = readindex();
	if ( version <= 61 )
		mesh.connects_jump = fpos
			+mesh.connects_count*sizeof(connect_t);
	mesh.connects = calloc(mesh.connects_count,sizeof(connect_t));
	memcpy(mesh.connects,pkgfile+fpos,mesh.connects_count
		*sizeof(connect_t));
	fpos = mesh.connects_jump;
	if ( version < 40 )
	{
		mesh.boundingboxes_count = readindex();
		mesh.boundingboxes = calloc(mesh.boundingboxes_count,
			sizeof(boundingbox_t));
		for ( uint32_t i=0; i<mesh.boundingboxes_count; i++ )
			READSTRUCT(mesh.boundingboxes[i],boundingbox_t)
	}
	else READSTRUCT(mesh.boundingbox2,boundingbox_t)
	if ( version > 61 ) READSTRUCT(mesh.boundingsphere2,boundingsphere_t)
	else if ( version >= 40 ) READSTRUCT(mesh.boundingsphere2,boundingsphere_61pre_t)
	if ( version > 61 ) mesh.vertlinks_jump = readdword();
	mesh.vertlinks_count = readindex();
	if ( version <= 61 )
		mesh.vertlinks_jump = fpos+mesh.vertlinks_count*4;
	mesh.vertlinks = calloc(mesh.vertlinks_count,4);
	memcpy(mesh.vertlinks,pkgfile+fpos,mesh.vertlinks_count*4);
	fpos = mesh.vertlinks_jump;
	mesh.textures_count = readindex();
	mesh.textures = calloc(mesh.textures_count,4);
	for ( uint32_t i=0; i<mesh.textures_count; i++ )
		mesh.textures[i] = readindex();
	if ( version >= 40 )
	{
		mesh.boundingboxes_count = readindex();
		mesh.boundingboxes = calloc(mesh.boundingboxes_count,
			sizeof(boundingbox_t));
		for ( uint32_t i=0; i<mesh.boundingboxes_count; i++ )
			READSTRUCT(mesh.boundingboxes[i],boundingbox_t)
		mesh.boundingspheres_count = readindex();
		mesh.boundingspheres = calloc(mesh.boundingspheres_count,
			sizeof(boundingsphere_t));
		for ( uint32_t i=0; i<mesh.boundingspheres_count; i++ )
		{
			if ( version > 61 )
				READSTRUCT(mesh.boundingspheres[i],boundingsphere_t)
			else READSTRUCT(mesh.boundingspheres[i],boundingsphere_61pre_t)
		}
	}
	else
	{
		// unknown data (various elements of size 6)
		uint32_t skipme = readindex();
		fpos += skipme*6;
	}
	mesh.frameverts = readdword();
	mesh.animframes = readdword();
	mesh.andflags = readdword();
	mesh.orflags = readdword();
	READSTRUCT(mesh.scale,vector_t)
	READSTRUCT(mesh.origin,vector_t)
	READSTRUCT(mesh.rotorigin,rotator_t)
	mesh.curpoly = readdword();
	mesh.curvertex = readdword();
	if ( version >= 66 )
	{
		mesh.texturelod_count = readindex();
		mesh.texturelods = calloc(mesh.texturelod_count,4);
		memcpy(mesh.texturelods,pkgfile+fpos,mesh.texturelod_count*4);
		fpos += mesh.texturelod_count*4;
	}
	else if ( version == 65 )
	{
		mesh.texturelod_count = 1;
		mesh.texturelods = calloc(1,4);
		mesh.texturelods[0] = readfloat();
	}
	else
	{
		mesh.texturelod_count = 0;
		mesh.texturelods = 0;
	}
	snprintf(fname,256,"%.*s.txt",namelen,name);
	f = fopen(fname,"w");
	printf(" Dumping Mesh structure to %s\n",fname);
	fprintf(f,"BoundingBox: (Min: (X=%g, Y=%g, Z=%g)),"
		" (Max: (X=%g, Y=%g, Z=%g)), IsValid: %hhu)\n",
		mesh.boundingbox.min.x,mesh.boundingbox.min.y,
		mesh.boundingbox.min.z,mesh.boundingbox.max.x,
		mesh.boundingbox.max.y,mesh.boundingbox.max.z,
		mesh.boundingbox.isvalid);
	fprintf(f,"BoundingSphere: (X=%g, Y=%g, Z=%g, W=%g)\n",
		mesh.boundingsphere.position.x,
		mesh.boundingsphere.position.y,
		mesh.boundingsphere.position.z,
		mesh.boundingsphere.w);
	fprintf(f,"Verts_Jump: %zu\n",mesh.verts_jump-prev);
	fprintf(f,"Verts_Count: %u\n",mesh.verts_count);
	for ( uint32_t i=0; i<mesh.verts_count; i++ )
	{
		if ( isdeusex )
			fprintf(f," Verts[%u]: (X=%hd, Y=%hd, Z=%hd, P=%hd)\n",
				i,mesh.verts_dx[i].x,mesh.verts_dx[i].y,
				mesh.verts_dx[i].z,mesh.verts_dx[i].pad);
		else fprintf(f," Verts[%u]: (X=%hd, Y=%hd, Z=%hd, V=0x%08x)\n",
				i,unpackuvert(mesh.verts_ue1[i],0),
				unpackuvert(mesh.verts_ue1[i],1),
				unpackuvert(mesh.verts_ue1[i],2),
				mesh.verts_ue1[i]);
	}
	fprintf(f,"Tris_Jump: %zu\n",mesh.tris_jump-prev);
	fprintf(f,"Tris_Count: %u\n",mesh.tris_count);
	for ( uint32_t i=0; i<mesh.tris_count; i++ )
		fprintf(f," Tri[%u]: (Verts: (%hu, %hu, %hu),"
				" UV: ((%hhu,%hhu),(%hhu,%hhu),(%hhu,%hhu)),"
				" Flags: 0x%08x, TexNum: %u)\n",i,
				mesh.tris[i].verts[0],mesh.tris[i].verts[1],
				mesh.tris[i].verts[2],mesh.tris[i].uv[0][0],
				mesh.tris[i].uv[0][1],mesh.tris[i].uv[1][0],
				mesh.tris[i].uv[1][1],mesh.tris[i].uv[2][0],
				mesh.tris[i].uv[2][1],mesh.tris[i].flags,
				mesh.tris[i].texnum);
	fprintf(f,"AnimSeqs_Count: %u\n",mesh.animseqs_count);
	for ( uint32_t i=0; i<mesh.animseqs_count; i++ )
	{
		int32_t l = 0;
		char *pname =
			(char*)(pkgfile+getname(mesh.animseqs[i].group,&l));
		fprintf(f," AnimSeq[%u]: (Group: \"%.*s\",",i,l,pname);
		pname = (char*)(pkgfile+getname(mesh.animseqs[i].name,&l));
		fprintf(f," Name: \"%.*s\", Start_Frame: %u,"
			" Num_Frames: %u, Function_Count: %u, Rate: %g)\n",l,
			pname,mesh.animseqs[i].start_frame,
			mesh.animseqs[i].num_frames,
			mesh.animseqs[i].function_count,
			mesh.animseqs[i].rate);
		for ( uint32_t j=0; j<mesh.animseqs[i].function_count; j++ )
		{
			pname = (char*)(pkgfile+getname(mesh.animseqs[i]
				.functions[j].function,&l));
			fprintf(f,"  Functions[%u]: (Time: %g,"
				" Function: %.*s)\n",j,
				mesh.animseqs[i].functions[j].time,l,pname);
		}
	}
	fprintf(f,"Connects_Jump: %zu\n",mesh.connects_jump-prev);
	fprintf(f,"Connects_Count: %u\n",mesh.connects_count);
	for ( uint32_t i=0; i<mesh.connects_count; i++ )
		fprintf(f," Connects[%u]: (NumVertTriangles: %u,"
			" TriangleListOffset: %u)\n",i,
			mesh.connects[i].numverttriangles,
			mesh.connects[i].trianglelistoffset);
	fprintf(f,"BoundingBox: (Min: (X=%g, Y=%g, Z=%g)),"
		" (Max: (X=%g, Y=%g, Z=%g), IsValid: %hhu)\n",
		mesh.boundingbox2.min.x,mesh.boundingbox2.min.y,
		mesh.boundingbox2.min.z,mesh.boundingbox2.max.x,
		mesh.boundingbox2.max.y,mesh.boundingbox2.max.z,
		mesh.boundingbox2.isvalid);
	fprintf(f,"BoundingSphere: (X=%g, Y=%g, Z=%g, W=%g)\n",
		mesh.boundingsphere2.position.x,
		mesh.boundingsphere2.position.y,
		mesh.boundingsphere2.position.z,
		mesh.boundingsphere2.w);
	fprintf(f,"VertLinks_Jump: %zu\n",mesh.vertlinks_jump-prev);
	fprintf(f,"VertLinks_Count: %u\n",mesh.vertlinks_count);
	for ( uint32_t i=0; i<mesh.vertlinks_count; i++ )
		fprintf(f," VertLinks[%u]: %u\n",i,mesh.vertlinks[i]);
	fprintf(f,"Textures_Count: %u\n",mesh.textures_count);
	for ( uint32_t i=0; i<mesh.textures_count; i++ )
	{
		fprintf(f," Textures[%u]: \"",i);
		construct_fullname(f,mesh.textures[i]);
		fprintf(f,"\"\n");
	}
	fprintf(f,"BoundingBoxes_Count: %u\n",mesh.boundingboxes_count);
	for ( uint32_t i=0; i<mesh.boundingboxes_count; i++ )
		fprintf(f," BoundingBoxes[%u]: (Min: (X=%g, Y=%g, Z=%g)),"
			" (Max: (X=%g, Y=%g, Z=%g)), IsValid: %hhu)\n",i,
			mesh.boundingboxes[i].min.x,
			mesh.boundingboxes[i].min.y,
			mesh.boundingboxes[i].min.z,
			mesh.boundingboxes[i].max.x,
			mesh.boundingboxes[i].max.y,
			mesh.boundingboxes[i].max.z,
			mesh.boundingboxes[i].isvalid);
	fprintf(f,"BoundingSpheres_Count: %u\n",mesh.boundingspheres_count);
	for ( uint32_t i=0; i<mesh.boundingspheres_count; i++ )
	fprintf(f," BoundingSpheres[%u]: (X=%g, Y=%g, Z=%g, W=%g)\n",i,
			mesh.boundingspheres[i].position.x,
			mesh.boundingspheres[i].position.y,
			mesh.boundingspheres[i].position.z,
			mesh.boundingspheres[i].w);
	fprintf(f,"FrameVerts: %u\n",mesh.frameverts);
	fprintf(f,"AnimFrames: %u\n",mesh.animframes);
	fprintf(f,"ANDFlags: 0x%08x\n",mesh.andflags);
	fprintf(f,"ORFlags: 0x%08x\n",mesh.orflags);
	fprintf(f,"Scale: (X=%g, Y=%g, Z=%g)\n",mesh.scale.x,mesh.scale.y,
		mesh.scale.z);
	fprintf(f,"Origin: (X=%g, Y=%g, Z=%g)\n",mesh.origin.x,mesh.origin.y,
		mesh.origin.z);
	fprintf(f,"RotOrigin: (Pitch=%d, Yaw=%d, Roll=%d)\n",
		mesh.rotorigin.pitch,mesh.rotorigin.yaw,mesh.rotorigin.roll);
	fprintf(f,"CurPoly: %u\n",mesh.curpoly);
	fprintf(f,"CurVertex: %u\n",mesh.curvertex);
	fprintf(f,"TextureLOD_Count: %u\n",mesh.texturelod_count);
	for ( uint32_t i=0; i<mesh.texturelod_count; i++ )
		fprintf(f," TextureLODs[%u]: %g\n",i,mesh.texturelods[i]);
	if ( !islodmesh ) goto finish;
	lodmesh.collapsepointthus_count = readindex();
	lodmesh.collapsepointthus =
		calloc(lodmesh.collapsepointthus_count,2);
	memcpy(lodmesh.collapsepointthus,pkgfile+fpos,
		lodmesh.collapsepointthus_count*2);
	fpos += lodmesh.collapsepointthus_count*2;
	lodmesh.facelevel_count = readindex();
	lodmesh.facelevels = calloc(lodmesh.facelevel_count,2);
	memcpy(lodmesh.facelevels,pkgfile+fpos,
		lodmesh.facelevel_count*2);
	fpos += lodmesh.facelevel_count*2;
	lodmesh.faces_count = readindex();
	lodmesh.faces = calloc(lodmesh.faces_count,sizeof(lodface_t));
	memcpy(lodmesh.faces,pkgfile+fpos,lodmesh.faces_count
		*sizeof(lodface_t));
	fpos += lodmesh.faces_count*sizeof(lodface_t);
	lodmesh.collapsewedgethus_count = readindex();
	lodmesh.collapsewedgethus =
		calloc(lodmesh.collapsewedgethus_count,2);
	memcpy(lodmesh.collapsewedgethus,pkgfile+fpos,
		lodmesh.collapsewedgethus_count*2);
	fpos += lodmesh.collapsewedgethus_count*2;
	lodmesh.wedges_count = readindex();
	lodmesh.wedges = calloc(lodmesh.wedges_count,sizeof(wedge_t));
	memcpy(lodmesh.wedges,pkgfile+fpos,lodmesh.wedges_count
		*sizeof(wedge_t));
	fpos += lodmesh.wedges_count*sizeof(wedge_t);
	lodmesh.materials_count = readindex();
	lodmesh.materials = calloc(lodmesh.materials_count,sizeof(material_t));
	memcpy(lodmesh.materials,pkgfile+fpos,lodmesh.materials_count
		*sizeof(material_t));
	fpos += lodmesh.materials_count*sizeof(material_t);
	lodmesh.specialfaces_count = readindex();
	lodmesh.specialfaces = calloc(lodmesh.specialfaces_count,
		sizeof(lodface_t));
	memcpy(lodmesh.specialfaces,pkgfile+fpos,lodmesh.specialfaces_count
		*sizeof(lodface_t));
	fpos += lodmesh.specialfaces_count*sizeof(lodface_t);
	lodmesh.modelverts = readdword();
	lodmesh.specialverts = readdword();
	lodmesh.meshscalemax = readfloat();
	lodmesh.lodhysteresis = readfloat();
	lodmesh.lodstrength = readfloat();
	lodmesh.lodminverts = readdword();
	lodmesh.lodmorph = readfloat();
	lodmesh.lodzdistance = readfloat();
	lodmesh.remapanimverts_count = readindex();
	lodmesh.remapanimverts = calloc(lodmesh.remapanimverts_count,2);
	memcpy(lodmesh.remapanimverts,pkgfile+fpos,lodmesh.remapanimverts_count
		*2);
	fpos += lodmesh.remapanimverts_count*2;
	lodmesh.oldframeverts = readdword();
	printf(" Dumping LodMesh structure to %s\n",fname);
	fprintf(f,"CollapsePointThus_Count: %u\n",
		lodmesh.collapsepointthus_count);
	for ( uint32_t i=0; i<lodmesh.collapsepointthus_count; i++ )
		fprintf(f," CollapsePointThus[%u]: %hu\n",i,
			lodmesh.collapsepointthus[i]);
	fprintf(f,"FaceLevel_Count: %u\n",lodmesh.facelevel_count);
	for ( uint32_t i=0; i<lodmesh.facelevel_count; i++ )
		fprintf(f," FaceLevels[%u]: %hu\n",i,lodmesh.facelevels[i]);
	fprintf(f,"Faces_Count: %u\n",lodmesh.faces_count);
	for ( uint32_t i=0; i<lodmesh.faces_count; i++ )
		fprintf(f," Faces[%u]: (Wedges: (%hu, %hu, %hu),"
			" Material: %hu)\n",i,lodmesh.faces[i].wedges[0],
			lodmesh.faces[i].wedges[1],
			lodmesh.faces[i].wedges[2],
			lodmesh.faces[i].material);
	fprintf(f,"CollapseWedgeThus_Count: %u\n",
		lodmesh.collapsewedgethus_count);
	for ( uint32_t i=0; i<lodmesh.collapsewedgethus_count; i++ )
		fprintf(f," CollapseWedgeThus[%u]: %hu\n",i,
			lodmesh.collapsewedgethus[i]);
	fprintf(f,"Wedges_Count: %u\n",lodmesh.wedges_count);
	for ( uint32_t i=0; i<lodmesh.wedges_count; i++ )
		fprintf(f," Wedges[%u]: (Vertex: %hu, ST: (%hhu, %hhu))\n",
			i,lodmesh.wedges[i].vertex,lodmesh.wedges[i].st[0],
			lodmesh.wedges[i].st[1]);
	fprintf(f,"Materials_Count: %u\n",lodmesh.materials_count);
	for ( uint32_t i=0; i<lodmesh.materials_count; i++ )
		fprintf(f," Materials[%u]: (Flags: 0x%08x, TexNum: %u)\n",i,
			lodmesh.materials[i].flags,
			lodmesh.materials[i].texnum);
	fprintf(f,"SpecialFaces_Count: %u\n",lodmesh.specialfaces_count);
	for ( uint32_t i=0; i<lodmesh.specialfaces_count; i++ )
		fprintf(f," SpecialFaces[%u]: (Wedges: (%hu, %hu, %hu),"
			" Material: %hu)\n",i,
			lodmesh.specialfaces[i].wedges[0],
			lodmesh.specialfaces[i].wedges[1],
			lodmesh.specialfaces[i].wedges[2],
			lodmesh.specialfaces[i].material);
	fprintf(f,"ModelVerts: %u\n",lodmesh.modelverts);
	fprintf(f,"SpecialVerts: %u\n",lodmesh.specialverts);
	fprintf(f,"MeshScaleMax: %g\n",lodmesh.meshscalemax);
	fprintf(f,"LODHysteresis: %g\n",lodmesh.lodhysteresis);
	fprintf(f,"LODStrength: %g\n",lodmesh.lodstrength);
	fprintf(f,"LODMinVerts: %u\n",lodmesh.lodminverts);
	fprintf(f,"LODZDistance: %g\n",lodmesh.lodmorph);
	fprintf(f,"LODMorph: %g\n",lodmesh.lodzdistance);
	fprintf(f,"RemapAnimVerts_Count: %u\n",lodmesh.remapanimverts_count);
	for ( uint32_t i=0; i<lodmesh.remapanimverts_count; i++ )
		fprintf(f," RemapAnimVerts[%u]: %hu\n",i,
			lodmesh.remapanimverts[i]);
	fprintf(f,"OldFrameVerts: %u\n",lodmesh.oldframeverts);
	// construct the true faces from the lodmesh data
	printf(" Converting LodMesh back to Mesh\n");
	mesh.tris_count = lodmesh.faces_count+lodmesh.specialfaces_count;
	mesh.tris = calloc(mesh.tris_count,sizeof(tri_t));
	uint32_t j = 0;
	for ( uint32_t i=0; i<lodmesh.specialfaces_count; i++ )
	{
		mesh.tris[i].verts[0] = j++;
		mesh.tris[i].verts[1] = j++;
		mesh.tris[i].verts[2] = j++;
		mesh.tris[i].uv[0][0] = 0;
		mesh.tris[i].uv[1][0] = 0;
		mesh.tris[i].uv[2][0] = 0;
		mesh.tris[i].uv[0][1] = 0;
		mesh.tris[i].uv[1][1] = 0;
		mesh.tris[i].uv[2][1] = 0;
		mesh.tris[i].flags = PF_SPECIALPOLY; // hack
		mesh.tris[i].texnum = 0;
	}
	for ( uint32_t i=0; i<lodmesh.faces_count; i++ )
	{
		j = lodmesh.specialfaces_count+i;
		// sweet mother of lord jesus look at all that hot mess
		mesh.tris[j].verts[0] =
			lodmesh.wedges[lodmesh.faces[i].wedges[0]].vertex
			+lodmesh.specialverts;
		mesh.tris[j].verts[1] =
			lodmesh.wedges[lodmesh.faces[i].wedges[1]].vertex
			+lodmesh.specialverts;
		mesh.tris[j].verts[2] =
			lodmesh.wedges[lodmesh.faces[i].wedges[2]].vertex
			+lodmesh.specialverts;
		mesh.tris[j].uv[0][0] =
			lodmesh.wedges[lodmesh.faces[i].wedges[0]].st[0];
		mesh.tris[j].uv[1][0] =
			lodmesh.wedges[lodmesh.faces[i].wedges[1]].st[0];
		mesh.tris[j].uv[2][0] =
			lodmesh.wedges[lodmesh.faces[i].wedges[2]].st[0];
		mesh.tris[j].uv[0][1] =
			lodmesh.wedges[lodmesh.faces[i].wedges[0]].st[1];
		mesh.tris[j].uv[1][1] =
			lodmesh.wedges[lodmesh.faces[i].wedges[1]].st[1];
		mesh.tris[j].uv[2][1] =
			lodmesh.wedges[lodmesh.faces[i].wedges[2]].st[1];
		mesh.tris[j].flags =
			lodmesh.materials[lodmesh.faces[i].material].flags;
		mesh.tris[j].texnum =
			lodmesh.materials[lodmesh.faces[i].material].texnum;
	}
finish:
	fclose(f);
	// export anivfile
	snprintf(fname,256,"%.*s_a.3d",namelen,name);
	f = fopen(fname,"wb");
	aniheader_t ahead =
	{
		.numframes = mesh.animframes,
		.framesize = isdeusex?(8*mesh.frameverts):(4*mesh.frameverts),
	};
	// rearrange vertices if a RemapAnimVerts array exists
	if ( islodmesh && (lodmesh.remapanimverts_count > 0) )
	{
		if ( isdeusex )
		{
			vert_dx_t *rmap = calloc(mesh.verts_count,8);
			for ( uint16_t i=0; i<mesh.animframes; i++ )
			for ( uint16_t j=0; j<mesh.frameverts; j++ )
			{
				rmap[j+i*mesh.frameverts] = mesh
					.verts_dx[lodmesh.remapanimverts[j]+i
					*mesh.frameverts];
			}
			free(mesh.verts_dx);
			mesh.verts_dx = rmap;
		}
		else
		{
			uint32_t *rmap = calloc(mesh.verts_count,4);
			for ( uint16_t i=0; i<mesh.animframes; i++ )
			for ( uint16_t j=0; j<mesh.frameverts; j++ )
			{
				rmap[j+i*mesh.frameverts] = mesh
					.verts_ue1[lodmesh.remapanimverts[j]+i
					*mesh.frameverts];
			}
			free(mesh.verts_ue1);
			mesh.verts_ue1 = rmap;
		}
	}
	printf(" Exporting anivfile %s (%hu frames of size %hu)\n",fname,
		ahead.numframes,ahead.framesize);
	fwrite(&ahead,sizeof(aniheader_t),1,f);
	if ( isdeusex ) fwrite(mesh.verts_dx,8,mesh.verts_count,f);
	else fwrite(mesh.verts_ue1,4,mesh.verts_count,f);
	fclose(f);
	// export datafile
	snprintf(fname,256,"%.*s_d.3d",namelen,name);
	f = fopen(fname,"wb");
	dataheader_t dhead;
	// gotta clean up all that bogus data
	memset(&dhead,0,sizeof(dataheader_t));
	dhead.numpolys = mesh.tris_count;
	dhead.numverts = mesh.frameverts;
	printf(" Exporting datafile %s (%hu polys, %hu verts)\n",fname,
		dhead.numpolys,dhead.numverts);
	fwrite(&dhead,sizeof(dataheader_t),1,f);
	for ( uint16_t i=0; i<dhead.numpolys; i++ )
	{
		datapoly_t dpoly =
		{
			.vertices =
			{
				mesh.tris[i].verts[0],
				mesh.tris[i].verts[1],
				mesh.tris[i].verts[2],
			},
			.type = typefromflags(mesh.tris[i].flags),
			.color = 0,
			.uv =
			{
				{mesh.tris[i].uv[0][0],mesh.tris[i].uv[0][1]},
				{mesh.tris[i].uv[1][0],mesh.tris[i].uv[1][1]},
				{mesh.tris[i].uv[2][0],mesh.tris[i].uv[2][1]},
			},
			.texnum = mesh.tris[i].texnum,
			.flags = 0,
		};
		fwrite(&dpoly,sizeof(datapoly_t),1,f);
	}
	fclose(f);
	// generate template .uc
	snprintf(fname,256,"%.*s.uc",namelen,name);
	f = fopen(fname,"w");
	printf(" Generating template script %s\n",fname);
	fprintf(f,"class %.*s extends Actor;\n\n",namelen,name);
	if ( islodmesh )
		fprintf(f,"#exec MESH IMPORT MESH=%.*s"
		" ANIVFILE=Models\\%.*s_a.3d DATAFILE=Models\\%.*s_d.3d\n",
		namelen,name,namelen,name,namelen,name);
	else
		fprintf(f,"#exec MESH IMPORT MESH=%.*s"
		" ANIVFILE=Models\\%.*s_a.3d DATAFILE=Models\\%.*s_d.3d"
		" MLOD=0\n",namelen,name,namelen,name,namelen,name);
	fprintf(f,"#exec MESH ORIGIN MESH=%.*s X=%g Y=%g Z=%g",namelen,name,
		mesh.origin.x,mesh.origin.y,mesh.origin.z);
	if ( mesh.rotorigin.pitch )
		fprintf(f," PITCH=%g",mesh.rotorigin.pitch/256.);
	if ( mesh.rotorigin.yaw )
		fprintf(f," YAW=%g",mesh.rotorigin.yaw/256.);
	if ( mesh.rotorigin.roll )
		fprintf(f," ROLL=%g",mesh.rotorigin.roll/256.);
	fprintf(f,"\n\n");
	for ( uint32_t i=0; i<mesh.animseqs_count; i++ )
	{
		int32_t l = 0, l2 = 0;
		char *gname =
			(char*)(pkgfile+getname(mesh.animseqs[i].group,&l));
		char *aname =
			(char*)(pkgfile+getname(mesh.animseqs[i].name,&l2));
		fprintf(f,"#exec MESH SEQUENCE MESH=%.*s SEQ=%.*s"
			" STARTFRAME=%u NUMFRAMES=%u",namelen,name,l2,aname,
			mesh.animseqs[i].start_frame,
			mesh.animseqs[i].num_frames);
		if ( mesh.animseqs[i].rate != 30. )
			fprintf(f," RATE=%g",mesh.animseqs[i].rate);
		if ( strncmp(gname,"None",l) )
			fprintf(f," GROUP=%.*s",l,gname);
		fprintf(f,"\n");
	}
	fprintf(f,"\n");
	fprintf(f,"#exec MESHMAP SCALE MESHMAP=%.*s X=%g Y=%g Z=%g\n",
		namelen,name,mesh.scale.x,mesh.scale.y,mesh.scale.z);
	for ( uint32_t i=0; i<mesh.textures_count; i++ )
	{
		if ( !mesh.textures[i] ) continue;
		fprintf(f,"#exec MESHMAP SETTEXTURE MESHMAP=%.*s NUM=%u"
			" TEXTURE=",namelen,name,i);
		construct_fullname(f,mesh.textures[i]);
		fprintf(f,"\n");
	}
	fprintf(f,"\n");
	int mfn = 0;
	for ( uint32_t i=0; i<mesh.animseqs_count; i++ )
	{
		if ( !mesh.animseqs[i].function_count ) continue;
		mfn = 1;
		int32_t l = 0;
		char *aname =
			(char*)(pkgfile+getname(mesh.animseqs[i].name,&l));
		for ( uint32_t j=0; j<mesh.animseqs[i].function_count; j++ )
		{
			int32_t l2 = 0;
			char *fname = (char*)(pkgfile+getname(mesh.animseqs[i]
				.functions[j].function,&l2));
			fprintf(f,"#exec MESH NOTIFY MESH=%.*s SEQ=%.*s "
				"TIME=%g FUNCTION=%.*s\n",namelen,name,l,aname,
				mesh.animseqs[i].functions[j].time,l2,fname);
		}
	}
	if ( mfn ) fprintf(f,"\n");
	// footer
	fprintf(f,"defaultproperties\n{\n}\n");
	fclose(f);
	// cleanup
	if ( mesh.verts_dx ) free(mesh.verts_dx);
	if ( mesh.verts_ue1 ) free(mesh.verts_ue1);
	free(mesh.tris);
	for ( uint32_t i=0; i<mesh.animseqs_count; i++ )
		free(mesh.animseqs[i].functions);
	free(mesh.animseqs);
	free(mesh.connects);
	free(mesh.vertlinks);
	free(mesh.textures);
	free(mesh.boundingboxes);
	free(mesh.boundingspheres);
	free(mesh.texturelods);
	if ( !islodmesh ) return;
	free(lodmesh.collapsepointthus);
	free(lodmesh.facelevels);
	free(lodmesh.faces);
	free(lodmesh.collapsewedgethus);
	free(lodmesh.wedges);
	free(lodmesh.materials);
	free(lodmesh.specialfaces);
	free(lodmesh.remapanimverts);
}

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		printf("Usage: umodelextract <archive>\n");
		return 1;
	}
	int fd = open(argv[1],O_RDONLY);
	if ( fd == -1 )
	{
		printf("Failed to open file %s: %s\n",argv[1],strerror(errno));
		return 1;
	}
	struct stat st;
	fstat(fd,&st);
	pkgfile = malloc(st.st_size);
	memset(pkgfile,0,st.st_size);
	head = (upkg_header_t*)pkgfile;
	int r = 0;
	do
	{
		r = read(fd,pkgfile+fpos,131072);
		if ( r == -1 )
		{
			close(fd);
			free(pkgfile);
			printf("Read failed for file %s: %s\n",argv[1],
				strerror(errno));
			return 4;
		}
		fpos += r;
	}
	while ( r > 0 );
	close(fd);
	fpos = 0;
	if ( head->magic != UPKG_MAGIC )
	{
		printf("File %s is not a valid unreal package!\n",argv[1]);
		free(pkgfile);
		return 2;
	}
	if ( !hasname("Mesh") && !hasname("LodMesh") )
	{
		printf("Package %s does not contain vertex meshes\n",argv[1]);
		free(pkgfile);
		return 4;
	}
	if ( head->pkgver == 73 )
		printf(" DS9: The Fallen / Klingon Honor Guard package"
			" detected.\nThese games use a completely different,"
			" undocumented mesh structure. For now only raw data"
			" will be exported.\n");
	// loop through exports and search for models
	fpos = head->oexports;
	for ( uint32_t i=0; i<head->nexports; i++ )
	{
		int32_t class, ofs, siz, name;
		readexport(&class,&ofs,&siz,&name);
		if ( (siz <= 0) || (class >= 0) ) continue;
		// get the class name
		class = -class-1;
		if ( (uint32_t)class > head->nimports ) continue;
		int32_t l = 0;
		char *n = (char*)(pkgfile+getname(getimport(class),&l));
		int ismesh = !strncmp(n,"Mesh",l),
			islodmesh = !strncmp(n,"LodMesh",l);
		if ( !ismesh && !islodmesh ) continue;
		char *mdl = (char*)(pkgfile+getname(name,&l));
		printf("%s found: %.*s\n",islodmesh?"LodMesh":"Mesh",l,mdl);
		int32_t mdll = l;
#ifdef _DEBUG
		char fname[256] = {0};
		snprintf(fname,256,"%.*s.object",mdll,mdl);
		printf(" Dumping full object data to %s\n",fname);
		FILE *f = fopen(fname,"wb");
		fwrite(pkgfile+ofs,siz,1,f);
		fclose(f);
#endif
		// begin reading data
		size_t prev = fpos;
		fpos = ofs;
		if ( head->pkgver < 45 ) fpos += 4;
		if ( head->pkgver < 55 ) fpos += 16;
		if ( head->pkgver <= 44 ) fpos -= 6;	// ???
		if ( head->pkgver == 45 ) fpos -= 2;	// ???
		if ( head->pkgver == 41 ) fpos += 2;	// ???
		if ( head->pkgver <= 35 ) fpos += 8;	// ???
		int32_t prop = readindex();
		if ( (uint32_t)prop >= head->nnames )
		{
			printf("Unknown property %d, skipping\n",prop);
			fpos = prev;
			continue;
		}
		char *pname = (char*)(pkgfile+getname(prop,&l));
retry:
		if ( strncasecmp(pname,"none",l) )
		{
			uint8_t info = readbyte();
			//int array = info&0x80;
			//int type = info&0xf;
			int psiz = (info>>4)&0x7;
			switch ( psiz )
			{
			case 0:
				psiz = 1;
				break;
			case 1:
				psiz = 2;
				break;
			case 2:
				psiz = 4;
				break;
			case 3:
				psiz = 12;
				break;
			case 4:
				psiz = 16;
				break;
			case 5:
				psiz = readbyte();
				break;
			case 6:
				psiz = readword();
				break;
			case 7:
				psiz = readdword();
				break;
			}
			/*printf(" prop %.*s (%u, %u, %u, %u)\n",l,pname,array,type,(info>>4)&7,psiz);
			if ( array && (type != 3) )
			{
				int idx = readindex();
				printf(" index: %d\n",idx);
			}
			if ( type == 10 )
			{
				int32_t tl, sn;
				sn = readindex();
				char *sname = (char*)(pkgfile+getname(sn,&tl));
				printf(" struct: %.*s\n",tl,sname);
			}*/
			fpos += psiz;
			prop = readindex();
			pname = (char*)(pkgfile+getname(prop,&l));
			goto retry;
		}
#ifdef _DEBUG
		snprintf(fname,256,"%.*s.%s",mdll,mdl,
			islodmesh?"lodmesh":"mesh");
		printf(" Dumping full mesh struct to %s\n",fname);
		f = fopen(fname,"wb");
		fwrite(pkgfile+fpos,siz-(fpos-ofs),1,f);
		fclose(f);
#endif
		if ( head->pkgver == 73 ) continue;
		savemodel(mdll,mdl,islodmesh,head->pkgver);
		fpos = prev;
	}
	free(pkgfile);
	return 0;
}
