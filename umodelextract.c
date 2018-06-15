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

#define readstruct(x,y) {memcpy(&x,pkgfile+fpos,sizeof(y));fpos+=sizeof(y);}

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
	if ( head->pkgver >= 60 ) fpos += 4;
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

void readexport( int32_t *class, int32_t *ofs, int32_t *siz, int32_t *name )
{
	*class = readindex();
	readindex();
	if ( head->pkgver >= 60 ) fpos += 4;
	*name = readindex();
	fpos += 4;
	*siz = readindex();
	if ( *siz > 0 ) *ofs = readindex();
}

typedef struct
{
	float x, y, z;
} __attribute__((packed)) vector_t;

typedef struct
{
	float pitch, yaw, roll;
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
	uint32_t time;
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
	boundingsphere_61pre_t boundingsphere;
	uint32_t verts_count;
	uint32_t *verts_ue1;
	vert_dx_t *verts_dx;
	uint32_t tris_count;
	tri_t *tris;
	uint32_t animseqs_count;
	animseq_t *animseqs;
	uint32_t connects_jump, connects_count;
	connect_t *connects;
	boundingbox_t boundingbox2;
	boundingsphere_61pre_t boundingsphere2;
	uint32_t vertlinks_jump, vertlinks_count;
	uint32_t *vertlinks;
	uint32_t textures_count;
	int32_t *textures;
	uint32_t boundingboxes_count;
	boundingbox_t *boundingboxes;
	uint32_t boundingspheres_count;
	boundingsphere_61pre_t *boundingspheres;
	uint32_t frameverts, animframes, andflags, orflags;
	vector_t scale, origin;
	rotator_t rotorigin;
	uint32_t curpoly, curvertex;
} __attribute__((packed)) mesh_61pre_t;

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
} __attribute__((packed)) mesh_pre65_t;

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
	float texturelod;
} __attribute__((packed)) mesh_65_t;

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
	uint16_t *collapsepointthuss;
	uint32_t facelevel_count;
	uint16_t *facelevels;
	uint32_t faces_count;
	lodface_t *faces;
	uint32_t collapsewedgethus_count;
	uint16_t *collapsewedgethuss;
	uint32_t wedges_count;
	wedge_t *wedges;
	uint32_t materials_count;
	material_t *materials;
	uint32_t specialfaces_count;
	wedge_t *specialfaces;
	uint32_t modelverts, specialverts;
	float meshscalemax, lodhysteresis, lodstrength;
	uint32_t lodminverts;
	float lodmorph, lodzdistance;
	uint32_t remapanimverts_count;
	uint16_t *remapanimverts;
	uint32_t oldframeverts;
} __attribute__((packed)) lodmesh_t;

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

// let the horrendous clusterfuck begin
void savemodel( char *name, int islodmesh, int version )
{
	(void)name;	// not used yet
	int isdeusex;
	if ( version >= 66 )
	{
		mesh_t mesh;
		lodmesh_t lodmesh;
		readstruct(mesh.boundingbox,boundingbox_t);
		readstruct(mesh.boundingsphere,boundingsphere_t);
		mesh.verts_jump = readdword();
		mesh.verts_count = readindex();
		// check for deus ex format
		mesh.verts_dx = 0;
		mesh.verts_ue1 = 0;
		if ( (mesh.verts_jump-fpos)/mesh.verts_count == 4 )
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
		mesh.tris_jump = readdword();
		mesh.tris_count = readindex();
		mesh.tris = calloc(mesh.tris_count,sizeof(tri_t));
		memcpy(mesh.tris,pkgfile+fpos,mesh.tris_count*sizeof(tri_t));
		fpos = mesh.tris_jump;
		mesh.animseqs_count = readindex();
		// this one can't be memcpy'd since there are index types
		mesh.animseqs = calloc(mesh.animseqs_count,sizeof(animseq_t));
		for ( uint32_t i=0; i<mesh.animseqs_count; i++ )
		{
			mesh.animseqs[i].name = readindex();
			mesh.animseqs[i].group = readindex();
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
					readdword();
				mesh.animseqs[i].functions[j].function =
					readindex();
			}
			mesh.animseqs[i].rate = readfloat();
		}
		mesh.connects_jump = readdword();
		mesh.connects_count = readindex();
		mesh.connects = calloc(mesh.connects_count,sizeof(connect_t));
		memcpy(mesh.connects,pkgfile+fpos,mesh.connects_count
			*sizeof(connect_t));
		fpos = mesh.connects_jump;
		readstruct(mesh.boundingbox2,boundingbox_t);
		readstruct(mesh.boundingsphere2,boundingsphere_t);
		mesh.vertlinks_jump = readdword();
		mesh.vertlinks_count = readindex();
		mesh.vertlinks = calloc(mesh.vertlinks_count,4);
		memcpy(mesh.vertlinks,pkgfile+fpos,mesh.vertlinks_count*4);
		fpos = mesh.vertlinks_jump;
		mesh.textures_count = readindex();
		mesh.textures = calloc(mesh.textures_count,4);
		for ( uint32_t i=0; i<mesh.textures_count; i++ )
			mesh.textures[i] = readindex();
		mesh.boundingboxes_count = readindex();
		mesh.boundingboxes = calloc(mesh.boundingboxes_count,
			sizeof(boundingbox_t));
		for ( uint32_t i=0; i<mesh.boundingboxes_count; i++ )
			readstruct(mesh.boundingboxes[i],boundingbox_t);
		mesh.boundingspheres_count = readindex();
		mesh.boundingspheres = calloc(mesh.boundingspheres_count,
			sizeof(boundingsphere_t));
		for ( uint32_t i=0; i<mesh.boundingspheres_count; i++ )
			readstruct(mesh.boundingspheres[i],boundingsphere_t);
		mesh.frameverts = readdword();
		mesh.animframes = readdword();
		mesh.andflags = readdword();
		mesh.orflags = readdword();
		readstruct(mesh.scale,vector_t);
		readstruct(mesh.origin,vector_t);
		readstruct(mesh.rotorigin,rotator_t);
		mesh.curpoly = readdword();
		mesh.curvertex = readdword();
		mesh.texturelod_count = readindex();
		mesh.texturelods = calloc(mesh.texturelod_count,4);
		memcpy(mesh.texturelods,pkgfile+fpos,mesh.texturelod_count*4);
		fpos += mesh.texturelod_count*4;
		if ( !islodmesh ) goto finish66;
		lodmesh.collapsepointthus_count = readindex();
		lodmesh.collapsepointthuss =
			calloc(lodmesh.collapsepointthus_count,2);
		memcpy(lodmesh.collapsepointthuss,pkgfile+fpos,
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
		// TODO JUST TAKING A BREAK, WILL CONTINUE LATER
finish66:
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
		free(lodmesh.collapsepointthuss);
		free(lodmesh.facelevels);
		free(lodmesh.faces);
		return;
	}
	else if ( version == 65 )
	{
		// TODO
	}
	else if ( version > 61 )
	{
		// TODO
	}
	else
	{
		// TODO
	}
	printf("Unsupported version %d\n",version);
}

int main( int argc, char **argv )
{
	if ( argc < 2 ) return 1;
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
	// loop through exports and search for music
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
		// begin reading data
		size_t prev = fpos;
		fpos = ofs;
		if ( head->pkgver < 40 ) fpos += 8;
		if ( head->pkgver < 60 ) fpos += 16;
		int32_t prop = readindex();
		if ( (uint32_t)prop >= head->nnames ) continue;
		char *pname = (char*)(pkgfile+getname(prop,&l));
		if ( strncasecmp(pname,"none",l) ) continue;
		savemodel(mdl,islodmesh,head->pkgver);
		fpos = prev;
	}
	free(pkgfile);
	return 0;
}
