#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include "protoagl.h"

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

pixel_t check[4] =
{
	{128,128,128,255},{255,255,255,255},
	{255,255,255,255},{128,128,128,255}
};

texture_t *tex;
model_t mesh;
int *uverts, nuverts;	// unreferenced vertices

vect_t meshpos = {0.f,0.f,-5.f,1.f}, meshrot = {0.f,0.f,0.f,0.f};
int meshframe = 0;
mat_t projection, translation, rotation;
buffer_t screen = {0,0,640,480,.01f,1000.f};

void rendermesh( void )
{
	// TODO
}

/* FPS helper code */
#define TMAX 64
int ti = 0;
float ts = 0.f, tl[TMAX] = {0.f};
float avg_fps( float nt )
{
	ts = ts-tl[ti]+nt;
	tl[ti] = nt;
	if ( ++ti == TMAX ) ti = 0;
	return ts/TMAX;
}
#define NANOS_SEC 1000000000L
long ticker( void )
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return ts.tv_nsec+ts.tv_sec*NANOS_SEC;
}

int main( int argc, char **argv )
{
	(void)argc, (void)argv;
/*	FILE *datafile, *anivfile;
	if ( argc < 3 )
	{
		fprintf(stderr,"usage: umeshview <anivfile> <datafile>\n");
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

	int *used = calloc(sizeof(int),256);
	int *refs = calloc(sizeof(int),dhead.numverts);
*/
	// TODO load mesh data

	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS);
	SDL_Window *win = SDL_CreateWindow("umeshview",SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,screen.width,screen.height,
		SDL_WINDOW_SHOWN);
	SDL_Surface *scr = SDL_GetWindowSurface(win);
	screen.color = malloc(sizeof(pixel_t)*screen.width*screen.height);
	screen.depth = malloc(sizeof(float)*screen.width*screen.height);
	SDL_Surface *fb = SDL_CreateRGBSurfaceFrom(screen.color,screen.width,
		screen.height,32,sizeof(pixel_t)*screen.width,0xFF0000,0xFF00,
		0xFF,0xFF000000);
	SDL_Event e;
	int active = 1;
	float frame = 0.f, fps = NAN, fpsmin = INFINITY, fpsmax = -INFINITY,
		fpsavg = 0.f;
	long tick, tock;
	float fh = tanf(90.f/360.f*PI)*screen.znear,
		fw = fh*(screen.width/(float)screen.height);
	frustum(&projection,-fw,fw,-fh,fh,screen.znear,screen.zfar);
	float rx = 0, rz = 0;
	while ( active )
	{
		tick = ticker();
		while ( SDL_PollEvent(&e) )
		{
			if ( e.type == SDL_QUIT ) active = 0;

		}
		agl_clearcolor(&screen,16,16,16);
		agl_cleardepth(&screen);
		mat_t rotx, rotz;
		rotate(&rotx,90.f*rx,ROT_Y);
		rotate(&rotz,90.f*rz,ROT_Z);
		mmul(&rotation,rotz,rotx);
		translate(&translation,meshpos);
		rendermesh();
		SDL_BlitSurface(fb,0,scr,0);
		SDL_UpdateWindowSurface(win);
		tock = ticker();
		frame = (float)(tock-tick)/NANOS_SEC;
		fps = 1.f/frame;
		fpsavg = avg_fps(fps);
		if ( fps > fpsmax ) fpsmax = fps;
		if ( fps < fpsmin ) fpsmin = fps;
		printf("FPS: %.2f (%.2f min, %.2f max, %.2f avg)\n",fps,fpsmin,
			fpsmax,fpsavg);
	}
	free(screen.color);
	free(screen.depth);
	SDL_FreeSurface(fb);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
