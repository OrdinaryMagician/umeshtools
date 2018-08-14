#include <epoxy/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

// standards suck sometimes
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#define SCR_FOV    90.f
#define SCR_ZNEAR  0.001f
#define SCR_ZFAR   1000.f
#define SCR_WIDTH  640
#define SCR_HEIGHT 480

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

const char *vsrc =
"";
const char *fsrc =
"";
const char *ugsrc =
"";
const char *uvsrc =
"";
const char *ufsrc =
"";

GLint mvbuf, mprog, uprog, mubuf;
GLint *gltex;

typedef struct
{
	float c[4][4];
} mat_t;
typedef struct
{
	float x, y, z;
} vect_t;
typedef struct
{
	int v[3];
	float uv[3][2];
	int type, texn;
} tri_t;
typedef struct
{
	tri_t *tris;
	int ntri;
	int type, texn;
} group_t;

vect_t *verts;
vect_t *norms;
int nvert;
tri_t *tris;
int ntri;
group_t *groups;
int ngroup;
int nframe;

int *uverts, nuverts;	// unreferenced vertices

// vector / matrix math helpers, taken from proto-alicegl
void vadd( vect_t *o, vect_t a, vect_t b )
{
	o->x = a.x+b.x;
	o->y = a.y+b.y;
	o->z = a.z+b.z;
}
void vsub( vect_t *o, vect_t a, vect_t b )
{
	o->x = a.x-b.x;
	o->y = a.y-b.y;
	o->z = a.z-b.z;
}
void vscale( vect_t *o, vect_t a, float b )
{
	o->x = a.x*b;
	o->y = a.y*b;
	o->z = a.z*b;
}
void cross( vect_t *a, vect_t b, vect_t c )
{
	a->x = b.y*c.z-b.z*c.y;
	a->y = b.z*c.x-b.x*c.z;
	a->z = b.x*c.y-b.y*c.x;
}
float vsize( vect_t v )
{
	return sqrtf(powf(v.x,2.f)+powf(v.y,2.f)+powf(v.z,2.f));
}
void normalize( vect_t *a )
{
	float s = vsize(*a);
	vscale(a,*a,s);
}
void frustum( mat_t *o, float left, float right, float bottom, float top,
	float near, float far )
{
	o->c[0][0] = (2.f*near)/(right-left);
	o->c[0][1] = 0.f;
	o->c[0][2] = (right+left)/(right-left);
	o->c[0][3] = 0.f;
	o->c[1][0] = 0.f;
	o->c[1][1] = (2.f*near)/(top-bottom);
	o->c[1][2] = (top+bottom)/(top-bottom);
	o->c[1][3] = 0.f;
	o->c[2][0] = 0.f;
	o->c[2][1] = 0.f;
	o->c[2][2] = (far+near)/(far-near);
	o->c[2][3] = -(2.f*far*near)/(far-near);
	o->c[3][0] = 0.f;
	o->c[3][1] = 0.f;
	o->c[3][2] = 1.f;
	o->c[3][3] = 0.f;
}
#define ROT_X 0
#define ROT_Y 1
#define ROT_Z 2
void rotate( mat_t *o, float angle, int axis )
{
	float theta = (angle/180.f)*M_PI;
	vect_t n = {0.f,0.f,0.f};
	if ( axis == 0 ) n.x = 1.f;
	else if ( axis == 1 ) n.y = 1.f;
	else n.z = 1.f;
	float s = sinf(theta);
	float c = cosf(theta);
	float oc = 1.f-c;
	o->c[0][0] = oc*n.x*n.x+c;
	o->c[0][1] = oc*n.x*n.y-n.z*s;
	o->c[0][2] = oc*n.z*n.x+n.y*s;
	o->c[0][3] = 0.f;
	o->c[1][0] = oc*n.x*n.y+n.z*s;
	o->c[1][1] = oc*n.y*n.y+c;
	o->c[1][2] = oc*n.y*n.z-n.x*s;
	o->c[1][3] = 0.f;
	o->c[2][0] = oc*n.z*n.x-n.y*s;
	o->c[2][1] = oc*n.y*n.z+n.x*s;
	o->c[2][2] = oc*n.z*n.z+c;
	o->c[2][3] = 0.f;
	o->c[3][0] = 0.f;
	o->c[3][1] = 0.f;
	o->c[3][2] = 0.f;
	o->c[3][3] = 1.f;
}
void mmul( mat_t *o, mat_t a, mat_t b )
{
	int i,j;
	for ( i=0; i<4; i++ ) for ( j=0; j<4; j++ )
		o->c[i][j] = a.c[i][0]*b.c[0][j]+a.c[i][1]*b.c[1][j]
			+a.c[i][2]*b.c[2][j]+a.c[i][3]*b.c[3][j];
}

// model matrix, just identity, no transforms needed
mat_t modelm =
{
	{{1,0,0,0},
	{0,1,0,0},
	{0,0,1,0},
	{0,0,0,1}},
};
// view matrix, currently set 5 units back from origin
mat_t viewm =
{
	{{1,0,0,0},
	{0,1,0,0},
	{0,0,1,0},
	{0,0,-5,1}},
};
// perspective matrix, set up in main()
mat_t projm;

float animframe = 0.f;

void prepare_vbuf( void )
{
}

void update_vbuf( void )
{
}

void rendermesh( void )
{

}

#define NANOS_SEC 1000000000L
long ticker( void )
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return ts.tv_nsec+ts.tv_sec*NANOS_SEC;
}

GLint compile_shader( GLenum type, const char *src )
{
	GLint hnd, len, suc;
	char *log;
	hnd = glCreateShader(type);
	len = strlen(src);
	glShaderSource(hnd,1,&src,&len);
	glCompileShader(hnd);
	glGetShaderiv(hnd,GL_COMPILE_STATUS,&suc);
	if ( !suc )
	{
		glGetShaderiv(hnd,GL_INFO_LOG_LENGTH,&len);
		log = malloc(len);
		glGetShaderInfoLog(hnd,len,&suc,log);
		fprintf(stderr,"Shader compile error:\n%s\n",log);
		free(log);
		return -1;
	}
	return hnd;
}

GLint link_shader( GLint geom, GLint vert, GLint frag )
{
	GLint hnd, suc, len;
	char *log;
	hnd = glCreateProgram();
	if ( geom != -1 ) glAttachShader(hnd,geom);
	if ( vert != -1 ) glAttachShader(hnd,vert);
	if ( frag != -1 ) glAttachShader(hnd,frag);
	glLinkProgram(hnd);
	glGetProgramiv(hnd,GL_LINK_STATUS,&suc);
	if ( !suc )
	{
		glGetShaderiv(hnd,GL_INFO_LOG_LENGTH,&len);
		log = malloc(len);
		glGetShaderInfoLog(hnd,len,&suc,log);
		fprintf(stderr,"Shader link error:\n%s\n",log);
		free(log);
		return -1;
	}
	return hnd;
}

// compare groups by render style
// sort order: normal->twosided->translucent->masked->modulated
// order is exactly as they are defined, so we can simply subtract them
int matcmp( const void *a, const void *b )
{
	int sa = ((const group_t*)a)->type&0x7,
		sb = ((const group_t*)b)->type&0x7;
	return sa-sb;
}

int mesh_load( const char *aniv, const char *data )
{
	FILE *datafile, *anivfile;
	if ( !(datafile = fopen(data,"rb")) )
	{
		fprintf(stderr,"Couldn't open datafile: %s\n",strerror(errno));
		return 2;
	}
	dataheader_t dhead;
	datapoly_t dpoly;
	aniheader_t ahead;
	uint32_t avert;
	int16_t dxvert[4];
	fread(&dhead,1,sizeof(dataheader_t),datafile);
	if ( feof(datafile) )
	{
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(datafile));
		fclose(datafile);
		return 8;
	}
	verts = 0;
	norms = 0;
	nvert = dhead.numverts;
	groups = malloc(0);
	ngroup = 0;
	tris = calloc(sizeof(tri_t),dhead.numpolys);
	ntri = dhead.numpolys;
	int *refs = calloc(sizeof(int),dhead.numverts);
	int c = -1;
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		fread(&dpoly,1,sizeof(datapoly_t),datafile);
		if ( feof(datafile) )
		{
			free(tris);
			free(refs);
			fprintf(stderr,"Premature end of file reached at "
				"%lu\n",ftell(datafile));
			fclose(datafile);
			return 8;
		}
		refs[dpoly.vertices[0]]++;
		refs[dpoly.vertices[1]]++;
		refs[dpoly.vertices[2]]++;
		tris[i].v[0] = dpoly.vertices[0];
		tris[i].v[1] = dpoly.vertices[1];
		tris[i].v[2] = dpoly.vertices[2];
		tris[i].uv[0][0] = dpoly.uv[0][0]/255.f;
		tris[i].uv[0][1] = dpoly.uv[0][1]/255.f;
		tris[i].uv[1][0] = dpoly.uv[1][0]/255.f;
		tris[i].uv[1][1] = dpoly.uv[1][1]/255.f;
		tris[i].uv[2][0] = dpoly.uv[2][0]/255.f;
		tris[i].uv[2][1] = dpoly.uv[2][1]/255.f;
		int texn = dpoly.texnum;
		int type = dpoly.type;
group_recheck:
		if ( c == -1 )
		{
			c = ngroup++;
			groups = realloc(groups,ngroup*sizeof(group_t));
			groups[c].texn = texn;
			groups[c].type = type;
			groups[c].tris = malloc(0);
			groups[c].ntri = 0;
		}
		else if ( (groups[c].texn != texn)
			|| (groups[c].type != type) )
		{
			c = -1;
			for ( int j=0; j<ngroup; j++ )
			{
				if ( (groups[j].texn != texn)
					|| (groups[j].type != type) )
					continue;
				c = j;
				break;
			}
			goto group_recheck;
		}
		groups[c].tris = realloc(groups[c].tris,sizeof(tri_t)*ntri+1);
		groups[c].tris[groups[c].ntri++] = tris[i];
	}
	// sort groups by render styles
	qsort(groups,ngroup,sizeof(group_t),matcmp);
	// populate unused verts
	nuverts = 0;
	for ( int i=0; i<dhead.numverts; i++ ) if ( !refs[i] ) nuverts++;
	uverts = calloc(sizeof(int),nuverts);
	int j = 0;
	for ( int i=0; i<dhead.numverts; i++ ) if ( !refs[i] ) uverts[j] = i;
	free(refs);
	fclose(datafile);
	if ( !(anivfile = fopen(aniv,"rb")) )
	{
		fprintf(stderr,"Couldn't open anivfile: %s\n",strerror(errno));
		return 4;
	}
	fread(&ahead,1,sizeof(ahead),anivfile);
	if ( feof(anivfile) )
	{
		free(tris);
		free(uverts);
		for ( int i=0; i<ngroup; i++ ) free(groups[i].tris);
		free(groups);
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	verts = calloc(sizeof(vect_t),dhead.numverts*ahead.numframes);
	norms = calloc(sizeof(vect_t),dhead.numverts*ahead.numframes);
	nframe = ahead.numframes;
	// check for Deus Ex's 16-bit vertex format
	int usedx;
	if ( (dhead.numverts*8) == ahead.framesize ) usedx = 1;
	else if ( (dhead.numverts*4) == ahead.framesize ) usedx = 0;
	else
	{
		free(verts);
		free(norms);
		free(tris);
		free(uverts);
		for ( int i=0; i<ngroup; i++ ) free(groups[i].tris);
		free(groups);
		fprintf(stderr," (Unknown format)\nIncorrect frame size,"
			" should be %u or %u. Wrong anivfile?\n",
			dhead.numverts*4,dhead.numverts*8);
		fclose(anivfile);
		return 16;
	}
	for ( int i=0; i<ahead.numframes; i++ )
	{
		int fs = dhead.numverts*i;
		// load vertices
		for ( int j=0; j<dhead.numverts; j++ )
		{
			if ( usedx ) fread(dxvert,4,sizeof(int16_t),anivfile);
			else fread(&avert,1,sizeof(avert),anivfile);
			if ( feof(anivfile) )
			{
				free(verts);
				free(norms);
				free(tris);
				free(uverts);
				for ( int i=0; i<ngroup; i++ )
					free(groups[i].tris);
				free(groups);
				fprintf(stderr,"Premature end of file reached "
					"at %lu\n",ftell(anivfile));
				fclose(anivfile);
				return 8;
			}
			if ( usedx )
			{
				verts[j+fs].x = dxvert[0]/32768.f;
				verts[j+fs].y = dxvert[1]/32768.f;
				verts[j+fs].z = dxvert[2]/32768.f;
			}
			else
			{
				verts[j+fs].x = unpackuvert(avert,0)/32768.f;
				verts[j+fs].y = unpackuvert(avert,1)/32768.f;
				verts[j+fs].z = unpackuvert(avert,2)/32768.f;
			}
		}
		// compute normals
		for ( int j=0; j<dhead.numverts; j++ )
		{
			vect_t nsum = {0};
			int t = 0;
			for ( int k=0; k<dhead.numpolys; k++ )
			{
				if ( (tris[k].v[0] != j)
					|| (tris[k].v[1] != j)
					|| (tris[k].v[2] != j) )
					continue;
				vect_t dir[2], norm;
				vsub(&dir[0],verts[tris[k].v[1]+fs],
					verts[tris[k].v[0]+fs]);
				vsub(&dir[1],verts[tris[k].v[2]+fs],
					verts[tris[k].v[0]+fs]);
				cross(&norm,dir[0],dir[1]);
				normalize(&norm);
				vadd(&nsum,nsum,norm);
				t++;
			}
			if ( t ) vscale(&nsum,nsum,1.f/t);
			norms[j] = nsum;
		}
	}
	return 0;
}

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: umeshview <anivfile> <datafile>\n");
		return 1;
	}
	int res = mesh_load(argv[1],argv[2]);
	if ( !res ) return res;
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,2);
	SDL_Window *win = SDL_CreateWindow("umeshview",SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,SCR_WIDTH,SCR_HEIGHT,
		SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	SDL_GLContext *ctx = SDL_GL_CreateContext(win);
	SDL_GL_SetSwapInterval(1);
	/* compile model and unreferenced vertex shader programs */
	GLint geom, vert, frag;
	if ( (frag=compile_shader(GL_FRAGMENT_SHADER,fsrc)) == -1 ) return 32;
	if ( (vert=compile_shader(GL_VERTEX_SHADER,vsrc)) == -1 ) return 32;
	if ( (mprog=link_shader(-1,vert,frag)) == -1 ) return 32;
	glDeleteShader(frag);
	glDeleteShader(vert);
	if ( (frag=compile_shader(GL_FRAGMENT_SHADER,ufsrc)) == -1 ) return 32;
	if ( (vert=compile_shader(GL_VERTEX_SHADER,uvsrc)) == -1 ) return 32;
	if ( (geom=compile_shader(GL_FRAGMENT_SHADER,ugsrc)) == -1 ) return 32;
	if ( (uprog=link_shader(geom,vert,frag)) == -1 ) return 32;
	glDeleteShader(frag);
	glDeleteShader(vert);
	glDeleteShader(geom);
	prepare_vbuf();
	SDL_Event e;
	int active = 1;
	float frame = 0.f;
	long tick, tock;
	float fh = tanf(SCR_FOV/360.f*M_PI)*SCR_ZNEAR,
		fw = fh*(SCR_WIDTH/(float)SCR_HEIGHT);
	frustum(&projm,-fw,fw,-fh,fh,SCR_ZNEAR,SCR_ZFAR);
	while ( active )
	{
		float rx = 0, ry = 0, rz = 0, mx = 0, my = 0, mz = 0;
		while ( SDL_PollEvent(&e) )
		{
			if ( e.type == SDL_QUIT ) active = 0;
			else if ( e.type == SDL_KEYDOWN )
			{
				if ( e.key.keysym.sym == SDLK_a )
					mx -= 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_d )
					mx += 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_q )
					my -= 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_e )
					my += 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_w )
					mz += 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_s )
					mz -= 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_LEFT )
					rx += 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_RIGHT )
					rx -= 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_PAGEUP )
					ry -= 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_PAGEDOWN )
					ry += 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_UP )
					rz -= 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_DOWN )
					rz += 1.f*frame;
				else if ( e.key.keysym.sym == SDLK_ESCAPE )
					active = 0;
			}
		}
		tick = ticker();
		mat_t rotx, roty, rotz;
		rotate(&rotx,rx,ROT_X);
		rotate(&roty,ry,ROT_Y);
		rotate(&rotz,rz,ROT_Z);
		mmul(&viewm,viewm,rotx);
		mmul(&viewm,viewm,roty);
		mmul(&viewm,viewm,rotz);
		viewm.c[3][0] += mx;
		viewm.c[3][1] += my;
		viewm.c[3][2] += mz;
		rendermesh();
		SDL_GL_SwapWindow(win);
		tock = ticker();
		frame = (float)(tock-tick)/NANOS_SEC;
		printf("FPS: %.2f\n",1.f/frame);
	}
	free(verts);
	free(norms);
	free(tris);
	for ( int i=0; i<ngroup; i++ ) free(groups[i].tris);
	free(groups);
	free(uverts);
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
