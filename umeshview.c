#include <epoxy/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <png.h>
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

#define SCR_FOV    45.f
#define SCR_ZNEAR  0.01f
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

typedef struct
{
	int16_t x, y, z, pad;
} __attribute__((packed)) dxvert_t;

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

#define PT_NORMAL        0
#define PT_TWOSIDED      1
#define PT_TRANSLUCENT   2
#define PT_MASKED        3
#define PT_MODULATED     4
#define PT_227ALPHABLEND 5
#define PT_UNUSED1       6
#define PT_UNUSED2       7
#define PT_MASK          0x07
#define PF_SPECIALTRI    0x08
#define PF_UNLIT         0x10
#define PF_CURVY         0x20
#define PF_227FLATSHADED 0x20
#define PF_MESHENVIROMAP 0x40
#define PF_NOSMOOTH      0x80
#define PF_MASK          0xF8

const char *vsrc =
"#version 430\n"
"\n"
"layout(location=0) in vec3 vPosition;\n"
"layout(location=1) in vec3 vNormal;\n"
"layout(location=2) in vec2 vCoord;\n"
"layout(location=3) in vec3 vPosition2;\n"
"layout(location=4) in vec3 vNormal2;\n"
"\n"
"out vec3 fVertex;\n"
"out vec3 fNormal;\n"
"out vec2 fCoord;\n"
"out vec3 fLight;\n"
"out vec2 eNormal;\n"
"\n"
"uniform mat4 MP;\n"
"uniform mat4 MV;\n"
"uniform float Interpolation;\n"
"uniform bool MeshEnviroMap;\n"
"\n"
"void main()\n"
"{\n"
"\tgl_Position.xyz = (1.-Interpolation)*vPosition+Interpolation*vPosition2;\n"
"\tgl_Position.w = 1.;\n"
"\tfVertex = (MV*gl_Position).xyz;\n"
"\tgl_Position = MP*MV*gl_Position;\n"
"\tfNormal = (1.-Interpolation)*vNormal+Interpolation*vNormal2;\n"
"\teNormal = normalize(MP*MV*vec4(fNormal,0.)).xy;\n"
"\tif ( MeshEnviroMap )\n"
"\t\tfCoord = fNormal.xz*.5+.5;\n"
"\telse\n"
"\t\tfCoord = vCoord;\n"
"\tfNormal = normalize(MV*vec4(fNormal,0.)).xyz;\n"
"\tfLight = (vec4(0.,0.,128.,0.)).xyz;\n"
"}\n";
const char *fsrc =
"#version 430\n"
"\n"
"in vec3 fVertex;\n"
"in vec3 fNormal;\n"
"in vec2 fCoord;\n"
"in vec3 fLight;\n"
"in vec3 fEye;\n"
"in vec2 eNormal;\n"
"\n"
"layout(location=0) out vec4 FragColor;\n"
"layout(binding=0) uniform sampler2D Texture;\n"
"layout(binding=1) uniform sampler2D Brightmap;\n"
"layout(binding=2) uniform sampler2D EnvMask;\n"
"layout(binding=3) uniform sampler2D EnvMap;\n"
"uniform bool Masked;\n"
"uniform bool Unlit;\n"
"uniform bool SWWMEnviroMap;\n"
"\n"
"void main()\n"
"{\n"
"\tvec4 res = texture2D(Texture,SWWMEnviroMap?(eNormal*vec2(.49,-.49)+vec2(.5)):fCoord);\n"
"\tif ( Masked && (res.a < .5) ) discard;\n"
"\tvec3 light = normalize(fLight-fVertex);\n"
"\tvec3 ref = normalize(-reflect(light,fNormal));\n"
"\tvec3 amb = vec3(.25);\n"
"\tvec3 diff = vec3(1.)*max(dot(fNormal,light),0.);\n"
"\tdiff = mix(diff,vec3(1.),texture2D(Brightmap,fCoord).x);\n"
"\tif ( !Unlit ) res.rgb *= amb+diff;\n"
"\tres.rgb += texture2D(EnvMap,eNormal*vec2(.49,-.49)+vec2(.5)).rgb*texture2D(EnvMask,fCoord).x;\n"
"\tFragColor = vec4(res.rgb,1.);\n"
"}\n";
const char *uvsrc =
"#version 430\n"
"\n"
"layout(location=0) in vec3 vPosition;\n"
"layout(location=1) in vec3 vPosition2;\n"
"\n"
"uniform mat4 MVP;\n"
"uniform float Interpolation;\n"
"\n"
"void main()\n"
"{\n"
"\tgl_Position.xyz = (1.-Interpolation)*vPosition+Interpolation*vPosition2;\n"
"\tgl_Position.w = 1.;\n"
"\tgl_Position = MVP*gl_Position;\n"
"}\n";
const char *ufsrc =
"#version 430\n"
"\n"
"layout(location=0) out vec4 FragColor;\n"
"\n"
"void main()\n"
"{\n"
"\tFragColor = vec4(1.,1.,0.,1.);\n"
"}\n";
const char *wvsrc =
"#version 430\n"
"\n"
"layout(location=0) in vec3 vPosition;\n"
"layout(location=1) in vec3 vPosition2;\n"
"layout(location=2) in vec3 vColor;\n"
"\n"
"uniform mat4 MVP;\n"
"uniform float Interpolation;\n"
"\n"
"out vec3 fColor;\n"
"\n"
"void main()\n"
"{\n"
"\tgl_Position.xyz = (1.-Interpolation)*vPosition+Interpolation*vPosition2;\n"
"\tgl_Position.w = 1.;\n"
"\tgl_Position = MVP*gl_Position;\n"
"\tfColor = vColor;\n"
"}\n";
const char *wfsrc =
"#version 430\n"
"\n"
"layout(location=0) out vec4 FragColor;\n"
"\n"
"in vec3 fColor;\n"
"\n"
"void main()\n"
"{\n"
"\tFragColor = vec4(fColor,1.);\n"
"}\n";

GLint mprog, uprog, wprog;
GLuint vao, vbuf, ubuf, wbuf;
GLuint btex = 0;
GLuint tex[256] = {0};
GLuint bmap[256] = {0};
GLuint emask[256] = {0};
GLuint emap[256] = {0};
int hide[256] = {0};
GLuint vmid, vmid2, viid, vtid, umid, uiid, venvid, vmskid, vunlid, vsenvid,
	wmid, wiid;
float bgcol[3] = {.5f,.5f,.5f};
int nosmooth = 0;
int swwmenviro = 0;

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
	vect_t *n;
	float uv[3][2];
	int type, texn;
} tri_t;
typedef struct
{
	tri_t **tris;
	int ntri;
	int type, texn;
} group_t;
typedef struct
{
	float p[3], n[3], c[2];
} vboe_t;
typedef struct
{
	float p[3];
} uvboe_t;
typedef struct
{
	float p[3], c[3];
} wvboe_t;

vect_t *verts;
vect_t *norms;
int nvert;
tri_t *tris;
int ntri;
group_t *groups;
int ngroup;
int nframe;

int *uverts, nuverts;	// unreferenced vertices
int wtri[3] = {-1};	// weapon triangle
int drawwtri = 0, drawuvert = 0;

// vector math helpers, taken from proto-alicegl
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
	if ( s < 1.f/65536.f ) s = 1.f/65536.f;
	vscale(a,*a,1.f/s);
}
void frustum( mat_t *o, float left, float right, float bottom, float top,
	float near, float far )
{
	o->c[0][0] = (2.f*near)/(right-left);
	o->c[1][0] = 0.f;
	o->c[2][0] = (right+left)/(right-left);
	o->c[3][0] = 0.f;
	o->c[0][1] = 0.f;
	o->c[1][1] = (2.f*near)/(top-bottom);
	o->c[2][1] = (top+bottom)/(top-bottom);
	o->c[3][1] = 0.f;
	o->c[0][2] = 0.f;
	o->c[1][2] = 0.f;
	o->c[2][2] = -(far+near)/(far-near);
	o->c[3][2] = -(2.f*far*near)/(far-near);
	o->c[0][3] = 0.f;
	o->c[1][3] = 0.f;
	o->c[2][3] = -1.f;
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
	o->c[1][0] = oc*n.x*n.y-n.z*s;
	o->c[2][0] = oc*n.z*n.x+n.y*s;
	o->c[3][0] = 0.f;
	o->c[0][1] = oc*n.x*n.y+n.z*s;
	o->c[1][1] = oc*n.y*n.y+c;
	o->c[2][1] = oc*n.y*n.z-n.x*s;
	o->c[3][1] = 0.f;
	o->c[0][2] = oc*n.z*n.x-n.y*s;
	o->c[1][2] = oc*n.y*n.z+n.x*s;
	o->c[2][2] = oc*n.z*n.z+c;
	o->c[3][2] = 0.f;
	o->c[0][3] = 0.f;
	o->c[1][3] = 0.f;
	o->c[2][3] = 0.f;
	o->c[3][3] = 1.f;
}
void mmul( mat_t *o, mat_t a, mat_t b )
{
	int i,j;
	for ( i=0; i<4; i++ ) for ( j=0; j<4; j++ )
		o->c[i][j] = a.c[i][0]*b.c[0][j]+a.c[i][1]*b.c[1][j]
			+a.c[i][2]*b.c[2][j]+a.c[i][3]*b.c[3][j];
}

float rotation[3] = {0.f,0.f,0.f};
float position[3] = {0.f,0.f,-128.f};
mat_t persp;

float animframe = 0.f, animrate = 30.f;
int framesize = 0;

void prepare_vbuf( void )
{
	// we actually need to define and bind a VAO otherwise it doesn't work
	glGenVertexArrays(1,&vao);
	glBindVertexArray(vao);
	// generate the vbo data for mesh
	size_t nvboe = 0;
	for ( int i=0; i<ngroup; i++ ) nvboe += groups[i].ntri*3;
	nvboe *= nframe;
	vboe_t *vboe = calloc(nvboe,sizeof(vboe_t));
	int m = 0;
	for ( int i=0; i<nframe; i++ )
	{
		for ( int j=0; j<ngroup; j++ )
		for ( int k=0; k<groups[j].ntri; k++ )
		{
			for ( int l=0; l<3; l++ )
			{
				vboe[m].p[0] = verts[groups[j].tris[k]->v[l]
					+i*nvert].x;
				vboe[m].p[1] = verts[groups[j].tris[k]->v[l]
					+i*nvert].y;
				vboe[m].p[2] = verts[groups[j].tris[k]->v[l]
					+i*nvert].z;
				if ( groups[j].type&PF_227FLATSHADED )
				{
					vboe[m].n[0] = groups[j].tris[k]->n[i].x;
					vboe[m].n[1] = groups[j].tris[k]->n[i].y;
					vboe[m].n[2] = groups[j].tris[k]->n[i].z;
				}
				else
				{
					vboe[m].n[0] = norms[groups[j].tris[k]
						->v[l]+i*nvert].x;
					vboe[m].n[1] = norms[groups[j].tris[k]
						->v[l]+i*nvert].y;
					vboe[m].n[2] = norms[groups[j].tris[k]
						->v[l]+i*nvert].z;
				}
				vboe[m].c[0] = groups[j].tris[k]->uv[l][0];
				vboe[m].c[1] = groups[j].tris[k]->uv[l][1];
				m++;
			}
		}
	}
	glGenBuffers(1,&vbuf);
	glBindBuffer(GL_ARRAY_BUFFER,vbuf);
	glBufferData(GL_ARRAY_BUFFER,sizeof(vboe_t)*nvboe,&vboe[0],
		GL_STATIC_DRAW);
	free(vboe);
	if ( !drawuvert ) goto skip_uvert;
	// generate the vbo data for scattered verts
	nvboe = nframe*nuverts;
	uvboe_t *uvboe = calloc(nvboe,sizeof(uvboe_t));
	int k = 0;
	for ( int i=0; i<nframe; i++ )
	{
		for ( int j=0; j<nuverts; j++ )
		{
			uvboe[k].p[0] = verts[uverts[j]+i*nvert].x;
			uvboe[k].p[1] = verts[uverts[j]+i*nvert].y;
			uvboe[k].p[2] = verts[uverts[j]+i*nvert].z;
			k++;
		}
	}
	glGenBuffers(1,&ubuf);
	glBindBuffer(GL_ARRAY_BUFFER,ubuf);
	glBufferData(GL_ARRAY_BUFFER,sizeof(uvboe_t)*nvboe,&uvboe[0],
		GL_STATIC_DRAW);
skip_uvert:
	if ( (wtri[0] == -1) || !drawwtri ) return;
	// generate the vbo data for weapon triangle lines
	// only needs 12 vertices per frame in total
	wvboe_t *wvboe = calloc(12*nframe,sizeof(wvboe_t));
	for ( int i=0; i<nframe; i++ )
	{
		// weapon triangle (magenta)
		for ( int j=0; j<6; j+=2 )
		{
			wvboe[j+i*12].p[0] = verts[wtri[j/2]+i*nvert].x;
			wvboe[j+i*12].p[1] = verts[wtri[j/2]+i*nvert].y;
			wvboe[j+i*12].p[2] = verts[wtri[j/2]+i*nvert].z;
			wvboe[j+i*12].c[0] = 255;
			wvboe[j+i*12].c[1] = 0;
			wvboe[j+i*12].c[2] = 255;
			wvboe[j+1+i*12].p[0] = verts[wtri[((j/2)+1)%3]+i*nvert].x;
			wvboe[j+1+i*12].p[1] = verts[wtri[((j/2)+1)%3]+i*nvert].y;
			wvboe[j+1+i*12].p[2] = verts[wtri[((j/2)+1)%3]+i*nvert].z;
			wvboe[j+1+i*12].c[0] = 255;
			wvboe[j+1+i*12].c[1] = 0;
			wvboe[j+1+i*12].c[2] = 255;
		}
		// attach coords (rgb)
		vect_t o, x, y, z;
		vadd(&o,verts[wtri[0]+i*nvert],verts[wtri[1]+i*nvert]);
		vscale(&o,o,.5f);
		vsub(&z,verts[wtri[0]+i*nvert],verts[wtri[1]+i*nvert]);
		normalize(&z);
		vect_t ac, ab;
		vsub(&ac,verts[wtri[2]+i*nvert],verts[wtri[0]+i*nvert]);
		vsub(&ab,verts[wtri[1]+i*nvert],verts[wtri[0]+i*nvert]);
		normalize(&ac);
		normalize(&ab);
		cross(&y,ac,ab);
		cross(&x,y,z);
		wvboe[6+i*12].p[0] = o.x;
		wvboe[6+i*12].p[1] = o.y;
		wvboe[6+i*12].p[2] = o.z;
		wvboe[6+i*12].c[0] = 255;
		wvboe[6+i*12].c[1] = 0;
		wvboe[6+i*12].c[2] = 0;
		wvboe[7+i*12].p[0] = o.x+x.x*8.f;
		wvboe[7+i*12].p[1] = o.y+x.y*8.f;
		wvboe[7+i*12].p[2] = o.z+x.z*8.f;
		wvboe[7+i*12].c[0] = 255;
		wvboe[7+i*12].c[1] = 0;
		wvboe[7+i*12].c[2] = 0;
		wvboe[8+i*12].p[0] = o.x;
		wvboe[8+i*12].p[1] = o.y;
		wvboe[8+i*12].p[2] = o.z;
		wvboe[8+i*12].c[0] = 0;
		wvboe[8+i*12].c[1] = 255;
		wvboe[8+i*12].c[2] = 0;
		wvboe[9+i*12].p[0] = o.x-y.x*8.f;	// right-handed y
		wvboe[9+i*12].p[1] = o.y-y.y*8.f;
		wvboe[9+i*12].p[2] = o.z-y.z*8.f;
		wvboe[9+i*12].c[0] = 0;
		wvboe[9+i*12].c[1] = 255;
		wvboe[9+i*12].c[2] = 0;
		wvboe[10+i*12].p[0] = o.x;
		wvboe[10+i*12].p[1] = o.y;
		wvboe[10+i*12].p[2] = o.z;
		wvboe[10+i*12].c[0] = 0;
		wvboe[10+i*12].c[1] = 0;
		wvboe[10+i*12].c[2] = 255;
		wvboe[11+i*12].p[0] = o.x+z.x*8.f;
		wvboe[11+i*12].p[1] = o.y+z.y*8.f;
		wvboe[11+i*12].p[2] = o.z+z.z*8.f;
		wvboe[11+i*12].c[0] = 0;
		wvboe[11+i*12].c[1] = 0;
		wvboe[11+i*12].c[2] = 255;
	}
	glGenBuffers(1,&wbuf);
	glBindBuffer(GL_ARRAY_BUFFER,wbuf);
	glBufferData(GL_ARRAY_BUFFER,sizeof(wvboe_t)*12*nframe,&wvboe[0],
		GL_STATIC_DRAW);
}

void rendermesh( void )
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glUseProgram(mprog);
	// prepare model view matrix
	mat_t mv =
	{
		{{1,0,0,0},
		{0,1,0,0},
		{0,0,1,0},
		{0,0,0,1}},
	};
	mat_t rotx, roty, rotz;
	rotate(&rotx,rotation[0],ROT_X);
	rotate(&roty,rotation[1],ROT_Y);
	rotate(&rotz,rotation[2],ROT_Z);
	mmul(&mv,mv,rotz);
	mmul(&mv,mv,roty);
	mmul(&mv,mv,rotx);
	mv.c[3][0] += position[0];
	mv.c[3][1] += position[1];
	mv.c[3][2] += position[2];
	glUniformMatrix4fv(vmid2,1,GL_FALSE,&mv.c[0][0]);
	glUniformMatrix4fv(vmid,1,GL_FALSE,&persp.c[0][0]);
	// full mvp matrix
	mat_t mvp;
	mmul(&mvp,persp,mv);
	int framea = floorf(animframe);
	glUniform1f(viid,animframe-framea);
	int frameb = ceilf(animframe);
	if ( frameb >= nframe ) frameb = 0;
	glBindBuffer(GL_ARRAY_BUFFER,vbuf);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(vboe_t),
		(char*)(framea*framesize*sizeof(vboe_t)));
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(vboe_t),
		(char*)(framea*framesize*sizeof(vboe_t)+12));
	glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(vboe_t),
		(char*)(framea*framesize*sizeof(vboe_t)+24));
	glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(vboe_t),
		(char*)(frameb*framesize*sizeof(vboe_t)));
	glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(vboe_t),
		(char*)(frameb*framesize*sizeof(vboe_t)+12));
	int vofs = 0;
	for ( int i=0; i<ngroup; i++ )
	{
		if ( (groups[i].type&PF_SPECIALTRI) || hide[groups[i].texn] )
		{
			// skip drawing weapon triangle / hidden parts
			vofs += groups[i].ntri*3;
			continue;
		}
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,tex[groups[i].texn]);
		if ( (groups[i].type&PF_NOSMOOTH) || nosmooth )
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		}
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D,bmap[groups[i].texn]?bmap[groups[i].texn]:btex);
		if ( (groups[i].type&PF_NOSMOOTH) || nosmooth )
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		}
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D,emask[groups[i].texn]?emask[groups[i].texn]:btex);
		if ( (groups[i].type&PF_NOSMOOTH) || nosmooth )
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		}
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D,emap[groups[i].texn]?emap[groups[i].texn]:btex);
		if ( (groups[i].type&PF_NOSMOOTH) || nosmooth )
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		}
		if ( (groups[i].type&PT_MASK) ) glDisable(GL_CULL_FACE);
		else glEnable(GL_CULL_FACE);
		glUniform1i(vsenvid,!!(groups[i].type&PF_MESHENVIROMAP)&&swwmenviro);
		glUniform1i(venvid,!!(groups[i].type&PF_MESHENVIROMAP));
		glUniform1i(vunlid,!!(groups[i].type&PF_UNLIT));
		glUniform1i(vmskid,(groups[i].type&PT_MASK)==PT_MASKED);
		// set blend mode when needed
		switch( groups[i].type&PT_MASK )
		{
		case PT_TRANSLUCENT:
			// additive
			glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_COLOR);
			glDepthMask(GL_FALSE);
			break;
		case PT_MODULATED:
			// modulated
			glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR);
			glDepthMask(GL_FALSE);
			break;
		default:
			glBlendFunc(GL_ONE,GL_ZERO);
			glDepthMask(GL_TRUE);
			break;
		}
		glDrawArrays(GL_TRIANGLES,vofs,groups[i].ntri*3);
		vofs += groups[i].ntri*3;
	}
	if ( !drawuvert ) goto skipdraw_uvert;
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_ONE,GL_ZERO);
	glDepthMask(GL_FALSE);
	glUseProgram(uprog);
	glUniformMatrix4fv(umid,1,GL_FALSE,&mvp.c[0][0]);
	glUniform1f(uiid,animframe-framea);
	glBindBuffer(GL_ARRAY_BUFFER,ubuf);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
	glDisableVertexAttribArray(4);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(uvboe_t),
		(char*)(framea*nuverts*sizeof(uvboe_t)));
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(uvboe_t),
		(char*)(frameb*nuverts*sizeof(uvboe_t)));
	glDrawArrays(GL_POINTS,0,nuverts);
skipdraw_uvert:
	if ( (wtri[0] == -1) || !drawwtri ) return;
	glUseProgram(wprog);
	glUniformMatrix4fv(wmid,1,GL_FALSE,&mvp.c[0][0]);
	glUniform1f(wiid,animframe-framea);
	glBindBuffer(GL_ARRAY_BUFFER,wbuf);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
	glDisableVertexAttribArray(4);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(wvboe_t),
		(char*)(framea*12*sizeof(wvboe_t)));
	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(wvboe_t),
		(char*)(frameb*12*sizeof(wvboe_t)));
	glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(wvboe_t),
		(char*)(framea*12*sizeof(wvboe_t)+12));
	glDrawArrays(GL_LINES,0,12);
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

typedef struct
{
	float ofsX, ofsY, ofsZ;
	int unmirror;
	int8_t pitch, yaw, roll;
	float scaleX, scaleY, scaleZ;
} transform_t;
transform_t tform =
{
	0.f, 0.f, 0.f,
	0,
	0, 64, 0,
	.05f, .05f, 0.1f
};

void transform( vect_t *v, transform_t *t )
{
	float pitch = (t->pitch/256.f)*2.f*acosf(-1.f),
		yaw = (t->yaw/256.f)*2.f*acosf(-1.f),
		roll = (t->roll/256.f)*2.f*acosf(-1.f);
	float s, c;
	vect_t v1, v2, v3, v4;
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
	v->x = v4.y;
	v->y = v4.z;
	v->z = v4.x;
}

// compare groups by render style
// sort order: normal->twosided->blended->masked->translucent->modulated
int matcmp( const void *a, const void *b )
{
	int ord[8] = {0,1,3,4,5,2,6,7};
	int sa = ord[((const group_t*)a)->type&PT_MASK],
		sb = ord[((const group_t*)b)->type&PT_MASK];
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
	datapoly_t *dpoly = 0;
	aniheader_t ahead;
	uint32_t *avert = 0;
	dxvert_t *dxvert = 0;
	fread(&dhead,sizeof(dataheader_t),1,datafile);
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
	tris = calloc(dhead.numpolys,sizeof(tri_t));
	ntri = dhead.numpolys;
	int *refs = calloc(dhead.numverts,sizeof(int));
	int c = -1;
	dpoly = calloc(dhead.numpolys,sizeof(datapoly_t));
	fread(dpoly,sizeof(datapoly_t),dhead.numpolys,datafile);
	if ( feof(datafile) )
	{
		free(dpoly);
		free(tris);
		free(refs);
		fprintf(stderr,"Premature end of file reached at "
			"%lu\n",ftell(datafile));
		fclose(datafile);
		return 8;
	}
	for ( int i=0; i<dhead.numpolys; i++ )
	{
		if ( dpoly[i].type&8 )
		{
			wtri[0] = dpoly[i].vertices[0];
			wtri[1] = dpoly[i].vertices[tform.unmirror?2:1];
			wtri[2] = dpoly[i].vertices[tform.unmirror?1:2];
		}
		refs[dpoly[i].vertices[0]]++;
		refs[dpoly[i].vertices[1]]++;
		refs[dpoly[i].vertices[2]]++;
		if ( tform.unmirror )
		{
			tris[i].v[0] = dpoly[i].vertices[0];
			tris[i].v[1] = dpoly[i].vertices[2];
			tris[i].v[2] = dpoly[i].vertices[1];
			tris[i].uv[0][0] = dpoly[i].uv[0][0]/255.f;
			tris[i].uv[0][1] = dpoly[i].uv[0][1]/255.f;
			tris[i].uv[1][0] = dpoly[i].uv[2][0]/255.f;
			tris[i].uv[1][1] = dpoly[i].uv[2][1]/255.f;
			tris[i].uv[2][0] = dpoly[i].uv[1][0]/255.f;
			tris[i].uv[2][1] = dpoly[i].uv[1][1]/255.f;
		}
		else
		{
			tris[i].v[0] = dpoly[i].vertices[0];
			tris[i].v[1] = dpoly[i].vertices[1];
			tris[i].v[2] = dpoly[i].vertices[2];
			tris[i].uv[0][0] = dpoly[i].uv[0][0]/255.f;
			tris[i].uv[0][1] = dpoly[i].uv[0][1]/255.f;
			tris[i].uv[1][0] = dpoly[i].uv[1][0]/255.f;
			tris[i].uv[1][1] = dpoly[i].uv[1][1]/255.f;
			tris[i].uv[2][0] = dpoly[i].uv[2][0]/255.f;
			tris[i].uv[2][1] = dpoly[i].uv[2][1]/255.f;
		}
		int texn = tris[i].texn = dpoly[i].texnum;
		int type = tris[i].type = dpoly[i].type;
		tris[i].n = 0;
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
		groups[c].tris = realloc(groups[c].tris,sizeof(tri_t*)*ntri+1);
		groups[c].tris[groups[c].ntri++] = &tris[i];
	}
	free(dpoly);
	// sort groups by render styles
	qsort(groups,ngroup,sizeof(group_t),matcmp);
	// populate unused verts
	nuverts = 0;
	for ( int i=0; i<dhead.numverts; i++ ) if ( !refs[i] ) nuverts++;
	uverts = calloc(nuverts,sizeof(int));
	int j = 0;
	for ( int i=0; i<dhead.numverts; i++ ) if ( !refs[i] ) uverts[j++] = i;
	free(refs);
	fclose(datafile);
	if ( !(anivfile = fopen(aniv,"rb")) )
	{
		fprintf(stderr,"Couldn't open anivfile: %s\n",strerror(errno));
		return 4;
	}
	fread(&ahead,sizeof(aniheader_t),1,anivfile);
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
	verts = calloc(dhead.numverts*ahead.numframes,sizeof(vect_t));
	norms = calloc(dhead.numverts*ahead.numframes,sizeof(vect_t));
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
		fprintf(stderr,"Incorrect frame size %u,"
			" should be %u or %u. Wrong anivfile?\n",
			ahead.framesize,dhead.numverts*4,dhead.numverts*8);
		fclose(anivfile);
		return 16;
	}
	if ( usedx )
	{
		dxvert = calloc(ahead.numframes*dhead.numverts,
			sizeof(dxvert_t));
		fread(dxvert,sizeof(dxvert_t),ahead.numframes*dhead.numverts,
			anivfile);
	}
	else
	{
		avert = calloc(ahead.numframes*dhead.numverts,
			sizeof(uint32_t));
		fread(avert,sizeof(uint32_t),ahead.numframes*dhead.numverts,
			anivfile);
	}
	if ( feof(anivfile) )
	{
		if ( usedx ) free(dxvert);
		else free(avert);
		free(verts);
		free(norms);
		free(tris);
		free(uverts);
		for ( int i=0; i<ngroup; i++ )
			free(groups[i].tris);
		free(groups);
		fprintf(stderr,"Premature end of file reached at %lu\n",
			ftell(anivfile));
		fclose(anivfile);
		return 8;
	}
	for ( int i=0; i<ahead.numframes; i++ )
	{
		int vs = dhead.numverts*i;
		// load vertices
		for ( int j=0; j<dhead.numverts; j++ )
		{
			if ( usedx )
			{
				verts[j+vs].x = dxvert[j+vs].x;
				verts[j+vs].y = dxvert[j+vs].y;
				verts[j+vs].z = dxvert[j+vs].z;
			}
			else
			{
				verts[j+vs].x = unpackuvert(avert[j+vs],0)/32.f;
				verts[j+vs].y = unpackuvert(avert[j+vs],1)/32.f;
				verts[j+vs].z = unpackuvert(avert[j+vs],2)/64.f;
			}
			transform(&verts[j+vs],&tform);
		}
		// compute facet normals
		for ( int j=0; j<dhead.numpolys; j++ )
		{
			vect_t dir[2], norm;
			vsub(&dir[0],verts[tris[j].v[1]+vs],
				verts[tris[j].v[0]+vs]);
			vsub(&dir[1],verts[tris[j].v[2]+vs],
				verts[tris[j].v[0]+vs]);
			cross(&norm,dir[0],dir[1]);
			normalize(&norm);
			if ( !tris[j].n ) tris[j].n = calloc(ahead.numframes,
				sizeof(vect_t));
			tris[j].n[i] = norm;
		}
		// compute vertex normals
		for ( int j=0; j<dhead.numverts; j++ )
		{
			vect_t nsum = {0};
			int t = 0;
			for ( int k=0; k<dhead.numpolys; k++ )
			{
				if ( (tris[k].v[0] != j)
					&& (tris[k].v[1] != j)
					&& (tris[k].v[2] != j) )
					continue;
				vadd(&nsum,nsum,tris[k].n[i]);
				t++;
			}
			vscale(&nsum,nsum,1.f/t);
			norms[j+vs] = nsum;
		}
	}
	if ( usedx ) free(dxvert);
	else free(avert);
	// calc frame size
	for ( int i=0; i<ngroup; i++ ) framesize += groups[i].ntri*3;
	return 0;
}

void tex_load( unsigned n, const char *filename, GLuint *dest )
{
	SDL_Surface *tx = IMG_Load(filename);
	if ( !tx ) return;
	// set first index to translucent
	if ( tx->format->palette ) tx->format->palette->colors[0].a = 0;
	SDL_Surface *txconv = SDL_ConvertSurfaceFormat(tx,
		SDL_PIXELFORMAT_RGBA32,0);
	glGenTextures(1,&dest[n]);
	glBindTexture(GL_TEXTURE_2D,dest[n]);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,txconv->w,txconv->h,0,GL_RGBA,
		GL_UNSIGNED_BYTE,txconv->pixels);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	SDL_FreeSurface(txconv);
	SDL_FreeSurface(tx);
}

int inputs[22] = {0};

int writepng( const char *filename, unsigned char *fdata, int fw, int fh )
{
	if ( !filename ) return 0;
	png_structp pngp;
	png_infop infp;
	FILE *pf;
	if ( !(pf = fopen(filename,"wb")) ) return 0;
	pngp = png_create_write_struct(PNG_LIBPNG_VER_STRING,0,0,0);
	if ( !pngp )
	{
		fclose(pf);
		return 0;
	}
	infp = png_create_info_struct(pngp);
	if ( !infp )
	{
		fclose(pf);
		png_destroy_write_struct(&pngp,0);
		return 0;
	}
	if ( setjmp(png_jmpbuf(pngp)) )
	{
		png_destroy_write_struct(&pngp,&infp);
		fclose(pf);
		return 0;
	}
	png_init_io(pngp,pf);
	png_set_IHDR(pngp,infp,fw,fh,8,PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);
	png_write_info(pngp,infp);
	// inverted Y, thanks OpenGL
	for ( int i=fh-1; i>=0; i-- ) png_write_row(pngp,fdata+(fw*4*i));
	png_write_end(pngp,infp);
	png_destroy_write_struct(&pngp,&infp);
	fclose(pf);
	return 1;
}

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: umeshview <anivfile> <datafile>"
			" [-f] [-p posX posY posZ] [-r pitch yaw roll]"
			" [-s scaleX scaleY scaleZ] [-t # <texture>|hide]"
			" [-drawweapon] [-drawunref] [-bgcolor XXXXXX]"
			" [-nosmooth] [-aasamples #] [-res #x#]"
			" [-altenvmap] [-b # <brightmap>]"
			" [-m # <envmask>] [-e # <envmap>]\n");
		return 1;
	}
	unsigned aasamples = 8;
	unsigned resx = SCR_WIDTH, resy = SCR_HEIGHT;
	char* tnames[256] = {0};
	char* bnames[256] = {0};
	char* mnames[256] = {0};
	char* enames[256] = {0};
	if ( argc > 3 )
	{
		for ( int i=3; i<argc; i++ )
		{
			if ( !strcmp(argv[i],"-p") )
			{
				if ( argc < i+3 ) break;
				sscanf(argv[++i],"%f",&tform.ofsX);
				sscanf(argv[++i],"%f",&tform.ofsY);
				sscanf(argv[++i],"%f",&tform.ofsZ);
			}
			else if ( !strcmp(argv[i],"-r") )
			{
				if ( argc < i+3 ) break;
				sscanf(argv[++i],"%hhd",&tform.pitch);
				sscanf(argv[++i],"%hhd",&tform.yaw);
				sscanf(argv[++i],"%hhd",&tform.roll);
			}
			else if ( !strcmp(argv[i],"-s") )
			{
				if ( argc < i+3 ) break;
				sscanf(argv[++i],"%f",&tform.scaleX);
				sscanf(argv[++i],"%f",&tform.scaleY);
				sscanf(argv[++i],"%f",&tform.scaleZ);
			}
			else if ( !strcmp(argv[i],"-f") )
			{
				tform.unmirror = 1;
				continue;
			}
			else if ( !strcmp(argv[i],"-t") )
			{
				if ( argc < i+2  ) continue;
				unsigned n = 0;
				sscanf(argv[++i],"%u",&n);
				char *tn = argv[++i];
				if ( !strcmp(tn,"hide") ) hide[n] = 1;
				else tnames[n] = tn;
			}
			else if ( !strcmp(argv[i],"-b") )
			{
				if ( argc < i+2  ) continue;
				unsigned n = 0;
				sscanf(argv[++i],"%u",&n);
				char *tn = argv[++i];
				bnames[n] = tn;
			}
			else if ( !strcmp(argv[i],"-m") )
			{
				if ( argc < i+2  ) continue;
				unsigned n = 0;
				sscanf(argv[++i],"%u",&n);
				char *tn = argv[++i];
				mnames[n] = tn;
			}
			else if ( !strcmp(argv[i],"-e") )
			{
				if ( argc < i+2  ) continue;
				unsigned n = 0;
				sscanf(argv[++i],"%u",&n);
				char *tn = argv[++i];
				enames[n] = tn;
			}
			else if ( !strcmp(argv[i],"-bgcolor") )
			{
				if ( argc < i+1  ) continue;
				unsigned r = 0, g = 0, b = 0;
				sscanf(argv[++i],"%02x%02x%02x",&r,&g,&b);
				bgcol[0] = r/255.f;
				bgcol[1] = g/255.f;
				bgcol[2] = b/255.f;
			}
			else if ( !strcmp(argv[i],"-res") )
			{
				if ( argc < i+1  ) continue;
				sscanf(argv[++i],"%ux%u",&resx,&resy);
			}
			else if ( !strcmp(argv[i],"-aasamples") )
			{
				if ( argc < i+1  ) continue;
				sscanf(argv[++i],"%u",&aasamples);
				if ( aasamples > 16 ) aasamples = 32;
				else if ( aasamples > 8 ) aasamples = 16;
				else if ( aasamples > 4 ) aasamples = 8;
				else if ( aasamples > 2 ) aasamples = 4;
				else if ( aasamples > 1 ) aasamples = 2;
			}
			else if ( !strcmp(argv[i],"-drawweapon") )
				drawwtri = 1;
			else if ( !strcmp(argv[i],"-drawunref") )
				drawuvert = 1;
			else if ( !strcmp(argv[i],"-nosmooth") )
				nosmooth = 1;
			else if ( !strcmp(argv[i],"-altenvmap") )
				swwmenviro = 1;
		}
	}
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS);
	IMG_Init(IMG_INIT_PNG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,aasamples);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
	SDL_Window *win = SDL_CreateWindow("umeshview",SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,resx,resy,
		SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
	SDL_GLContext *ctx = SDL_GL_CreateContext(win);
	SDL_GL_SetSwapInterval(1);
	float fh = tanf(SCR_FOV/360.f*M_PI)*SCR_ZNEAR,
		fw = fh*(resx/(float)resy);
	frustum(&persp,-fw,fw,-fh,fh,SCR_ZNEAR,SCR_ZFAR);
	int res = mesh_load(argv[1],argv[2]);
	if ( res ) return res;
	/* base blank brightmap */
	unsigned char blacktex[3] = {0,0,0};
	glGenTextures(1,&btex);
	glBindTexture(GL_TEXTURE_2D,btex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGB,GL_UNSIGNED_BYTE,
		&blacktex);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	for ( int i=0; i<256; i++ )
	{
		int skipme = 1;
		for ( int j=0; j<ngroup; j++ )
		{
			if ( groups[j].texn != i ) continue;
			skipme = 0;
			break;
		}
		if ( skipme ) continue;
		if ( bnames[i] ) tex_load(i,bnames[i],&bmap[0]);
		if ( mnames[i] ) tex_load(i,mnames[i],&emask[0]);
		if ( enames[i] ) tex_load(i,enames[i],&emap[0]);
		if ( tnames[i] )
		{
			tex_load(i,tnames[i],&tex[0]);
			continue;
		}
		unsigned char gentex[3] = {rand()%256,rand()%256,rand()%256};
		glGenTextures(1,&tex[i]);
		glBindTexture(GL_TEXTURE_2D,tex[i]);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGB,
			GL_UNSIGNED_BYTE,&gentex);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	}
	/* compile model and unreferenced vertex shader programs */
	GLint vert, frag;
	if ( (frag=compile_shader(GL_FRAGMENT_SHADER,fsrc)) == -1 ) return 32;
	if ( (vert=compile_shader(GL_VERTEX_SHADER,vsrc)) == -1 ) return 32;
	if ( (mprog=link_shader(-1,vert,frag)) == -1 ) return 32;
	glDeleteShader(frag);
	glDeleteShader(vert);
	vmid = glGetUniformLocation(mprog,"MP");
	vmid2 = glGetUniformLocation(mprog,"MV");
	viid = glGetUniformLocation(mprog,"Interpolation");
	venvid = glGetUniformLocation(mprog,"MeshEnviroMap");
	vsenvid = glGetUniformLocation(mprog,"SWWMEnviroMap");
	vmskid = glGetUniformLocation(mprog,"Masked");
	vunlid = glGetUniformLocation(mprog,"Unlit");
	if ( (frag=compile_shader(GL_FRAGMENT_SHADER,ufsrc)) == -1 ) return 32;
	if ( (vert=compile_shader(GL_VERTEX_SHADER,uvsrc)) == -1 ) return 32;
	if ( (uprog=link_shader(-1,vert,frag)) == -1 ) return 32;
	glDeleteShader(frag);
	glDeleteShader(vert);
	umid = glGetUniformLocation(uprog,"MVP");
	uiid = glGetUniformLocation(uprog,"Interpolation");
	if ( (frag=compile_shader(GL_FRAGMENT_SHADER,wfsrc)) == -1 ) return 32;
	if ( (vert=compile_shader(GL_VERTEX_SHADER,wvsrc)) == -1 ) return 32;
	if ( (wprog=link_shader(-1,vert,frag)) == -1 ) return 32;
	glDeleteShader(frag);
	glDeleteShader(vert);
	wmid = glGetUniformLocation(wprog,"MVP");
	wiid = glGetUniformLocation(wprog,"Interpolation");
	glPointSize(4.f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_BACK);
	glClearColor(bgcol[0],bgcol[1],bgcol[2],0.f);
	glClearDepth(1.f);
	prepare_vbuf();
	SDL_Event e;
	int active = 1;
	float frame = 0.f;
	long tick, tock;
	while ( active )
	{
		while ( SDL_PollEvent(&e) )
		{
			if ( e.type == SDL_QUIT ) active = 0;
			else if ( (e.type == SDL_KEYDOWN)
				|| (e.type == SDL_KEYUP) )
			{
				if ( e.key.keysym.sym == SDLK_a )
					inputs[0] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_d )
					inputs[1] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_q )
					inputs[2] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_e )
					inputs[3] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_w )
					inputs[4] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_s )
					inputs[5] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_LEFT )
					inputs[6] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_RIGHT )
					inputs[7] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_PAGEUP )
					inputs[8] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_PAGEDOWN )
					inputs[9] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_UP )
					inputs[10] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_DOWN )
					inputs[11] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_END )
					inputs[12] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_HOME )
					inputs[13] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_INSERT )
					inputs[14] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_DELETE )
					inputs[15] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_SPACE )
					inputs[16] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_RETURN )
					inputs[17] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_BACKSPACE )
					inputs[18] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_ESCAPE )
					inputs[19] = (e.type==SDL_KEYDOWN);
				else if ( e.key.keysym.sym == SDLK_PRINTSCREEN )
					inputs[21] = (e.type==SDL_KEYDOWN);
				if ( e.key.keysym.mod&KMOD_LSHIFT )
					inputs[20] = 1;
				else inputs[20] = 0;
			}
		}
		if ( inputs[0] ) position[0] += inputs[20]?.1f:1.f;
		if ( inputs[1] ) position[0] -= inputs[20]?.1f:1.f;
		if ( inputs[2] ) position[1] += inputs[20]?.1f:1.f;
		if ( inputs[3] ) position[1] -= inputs[20]?.1f:1.f;
		if ( inputs[4] ) position[2] += inputs[20]?.1f:1.f;
		if ( inputs[5] ) position[2] -= inputs[20]?.1f:1.f;
		if ( inputs[6] ) rotation[1] += inputs[20]?1.f:5.f;
		if ( inputs[7] ) rotation[1] -= inputs[20]?1.f:5.f;
		if ( inputs[8] ) rotation[2] += inputs[20]?1.f:5.f;
		if ( inputs[9] ) rotation[2] -= inputs[20]?1.f:5.f;
		if ( inputs[10] ) rotation[0] += inputs[20]?1.f:5.f;
		if ( inputs[11] ) rotation[0] -= inputs[20]?1.f:5.f;
		if ( inputs[12] )
		{
			inputs[12] = 0;
			rotation[0] = 0.f;
			rotation[1] = 0.f;
			rotation[2] = 0.f;
			position[0] = 0.f;
			position[1] = 0.f;
			position[2] = -128.f;
		}
		if ( inputs[13] )
		{
			inputs[13] = 0;
			animframe = 0.f;
			animrate = 30.f;
		}
		if ( inputs[14] ) animrate += 1.f;
		if ( inputs[15] )
			animrate = (animrate-1.f<1.f)?1.f:(animrate-1.f);
		if ( inputs[16] )
		{
			inputs[16] = 0;
			animrate *= -1.f;
		}
		if ( inputs[17] )
		{
			inputs[17] = 0;
			animframe = floorf(animframe)+1.f;
			if ( animframe >= nframe )
				animframe = 0;
		}
		if ( inputs[18] )
		{
			inputs[18] = 0;
			animframe = floorf(animframe)-1.f;
			if ( animframe < 0 )
				animframe = nframe-1;
		}
		if ( inputs[19] ) active = 0;
		if ( inputs[21] )
		{
			inputs[21] = 0;
			uint8_t *tmp = malloc(resx*resy*4);
			glReadPixels(0,0,resx,resy,GL_RGBA,GL_UNSIGNED_BYTE,tmp);
			char fname[256] = {0};
// fucking windows and its stupid nonsense
#ifdef _WIN64
			snprintf(fname,255,"%s\\umeshview%lld.png",getenv("TEMP"),time(0));
#elif defined(_WIN32)
			snprintf(fname,255,"%s\\umeshview%ld.png",getenv("TEMP"),time(0));
#else
			// does the job for me
			snprintf(fname,255,"/tmp/umeshview%lu.png",time(0));
#endif
			writepng(fname,tmp,resx,resy);
			free(tmp);
			printf("screenshot saved\n");
		}
		tick = ticker();
		rendermesh();
		SDL_GL_SwapWindow(win);
		tock = ticker();
		frame = (float)(tock-tick)/NANOS_SEC;
		printf("FPS: %g\tFrame: %g\tRate: %g"
			"\tPos: %g, %g, %g\tRot: %g %g %g\n",
			1.f/frame,animframe,animrate,position[0],position[1],
			position[2],rotation[0],rotation[1],rotation[2]);
		if ( animrate > 0.f ) animframe += frame*animrate;
		if ( animframe >= nframe ) animframe = fmodf(animframe,nframe);
	}
	free(verts);
	free(norms);
	for ( int i=0; i<ntri; i++ ) free(tris[i].n);
	free(tris);
	for ( int i=0; i<ngroup; i++ ) free(groups[i].tris);
	free(groups);
	free(uverts);
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	IMG_Quit();
	SDL_Quit();
	return 0;
}
