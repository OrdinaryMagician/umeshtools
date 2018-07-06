//=============================================================================
//
// Proto-AliceGL stuff
//
//=============================================================================

typedef struct
{
	float x, y, z, w;
} vect_t;
typedef struct
{
	float s, t;
} coord_t;
typedef struct
{
	float c[4][4];
} mat_t;
typedef struct
{
	float x, y, d;
} px_t;
typedef struct
{
	unsigned char b,g,r,a;
} __attribute__((packed)) pixel_t;
typedef struct
{
	int v[3], n[3], t[3], c[3], m;
} face_t;
typedef struct
{
	pixel_t *color;
	float *depth;
	int width, height;
	float znear, zfar;
} buffer_t;
typedef struct
{
	pixel_t *data;
	int width, height;
	int meta;
} texture_t;
typedef struct
{
	vect_t v[3], w[3], n[3];
	coord_t t[3];
	vect_t c[3];
	texture_t *m;
} tri_t;
typedef struct
{
	buffer_t *screen;
	px_t fragdata;
	vect_t position, normal;
	coord_t txcoord;
	vect_t color;
	texture_t *texture;
} frag_t;
typedef void(prog_t)( frag_t data );
typedef struct
{
	vect_t *vertices, *normals;
	coord_t *txcoords;
	vect_t *colors;
	texture_t *textures;
	face_t *triangles;
	int nvert, nnorm, ncoord, ncolor, ntex, ntri, nframe;
} model_t;

#ifdef __SSE3__
void vadd( vect_t *o, vect_t a, vect_t b )
{
	__m128 ma, mb, mo;
	ma = _mm_load_ps((const float*)&a);
	mb = _mm_load_ps((const float*)&b);
	mo = _mm_add_ps(ma,mb);
	_mm_store_ps((float*)o,mo);
}
void vsub( vect_t *o, vect_t a, vect_t b )
{
	__m128 ma, mb, mo;
	ma = _mm_load_ps((const float*)&a);
	mb = _mm_load_ps((const float*)&b);
	mo = _mm_sub_ps(ma,mb);
	_mm_store_ps((float*)o,mo);
}
void vmul( vect_t *o, vect_t a, vect_t b )
{
	__m128 ma, mb, mo;
	ma = _mm_load_ps((const float*)&a);
	mb = _mm_load_ps((const float*)&b);
	mo = _mm_mul_ps(ma,mb);
	_mm_store_ps((float*)o,mo);
}
void vdiv( vect_t *o, vect_t a, vect_t b )
{
	__m128 ma, mb, mo;
	ma = _mm_load_ps((const float*)&a);
	mb = _mm_load_ps((const float*)&b);
	mo = _mm_div_ps(ma,mb);
	_mm_store_ps((float*)o,mo);
}
void vscale( vect_t *o, vect_t a, float b )
{
	__m128 ma, mb, mo;
	ma = _mm_load_ps((const float*)&a);
	mb = _mm_set1_ps(b);
	mo = _mm_mul_ps(ma,mb);
	_mm_store_ps((float*)o,mo);
}
void vmat( vect_t *o, mat_t a, vect_t b )
{
	__m128 ma0, ma1, ma2, ma3, mb, mo0, mo1, mo2, mo3, mo4, mo5, mo;
	ma0 = _mm_load_ps((const float*)&a);
	ma1 = _mm_load_ps(((const float*)&a)+4);
	ma2 = _mm_load_ps(((const float*)&a)+8);
	ma3 = _mm_load_ps(((const float*)&a)+12);
	mb = _mm_load_ps((const float*)&b);
	mo0 = _mm_mul_ps(ma0,mb);
	mo1 = _mm_mul_ps(ma1,mb);
	mo2 = _mm_mul_ps(ma2,mb);
	mo3 = _mm_mul_ps(ma3,mb);
	mo4 = _mm_hadd_ps(mo0,mo1);
	mo5 = _mm_hadd_ps(mo2,mo3);
	mo = _mm_hadd_ps(mo4,mo5);
	_mm_store_ps((float*)o,mo);
}
void mmul( mat_t *o, mat_t a, mat_t b )
{
	__m128 ma0, ma1, ma2, ma3, mb0, mb1, mb2, mb3, mo;
	mb0 = _mm_load_ps((const float*)&b);
	mb1 = _mm_load_ps(((const float*)&b)+4);
	mb2 = _mm_load_ps(((const float*)&b)+8);
	mb3 = _mm_load_ps(((const float*)&b)+12);
	for ( int i=0; i<4; i++ )
	{
		ma0 = _mm_set1_ps(a.c[i][0]);
		ma1 = _mm_set1_ps(a.c[i][1]);
		ma2 = _mm_set1_ps(a.c[i][2]);
		ma3 = _mm_set1_ps(a.c[i][3]);
		mo = _mm_add_ps(_mm_add_ps(_mm_mul_ps(ma0,mb0),
			_mm_mul_ps(ma1,mb1)),_mm_add_ps(_mm_mul_ps(ma2,mb2),
			_mm_mul_ps(ma3,mb3)));
		_mm_store_ps((float*)o+(4*i),mo);
	}
}
void mscale( mat_t *o, mat_t a, float b )
{
	__m128 ma0, ma1, ma2, ma3, mb, mo;
	ma0 = _mm_load_ps((const float*)&a);
	ma1 = _mm_load_ps(((const float*)&a)+4);
	ma2 = _mm_load_ps(((const float*)&a)+8);
	ma3 = _mm_load_ps(((const float*)&a)+12);
	mb = _mm_set1_ps(b);
	mo = _mm_mul_ps(ma0,mb);
	_mm_store_ps((float*)o,mo);
	mo = _mm_mul_ps(ma1,mb);
	_mm_store_ps((float*)o+4,mo);
	mo = _mm_mul_ps(ma2,mb);
	_mm_store_ps((float*)o+8,mo);
	mo = _mm_mul_ps(ma3,mb);
	_mm_store_ps((float*)o+12,mo);
}
float dot( vect_t a, vect_t b )
{
	__m128 va, vb, vd;
	va = _mm_load_ps((const float*)&a);
	vb = _mm_load_ps((const float*)&b);
#ifdef __SSE4_1__
	vd = _mm_dp_ps(va,vb,0x77);
#else
	__m128 v0, v1;
	v0 = _mm_mul_ps(va,vb);
	v1 = _mm_hadd_ps(v0,v0);
	dp = _mm_hadd_ps(v1,v1);
#endif
	return _mm_cvtss_f32(vd);
}
float vsize( vect_t a )
{
	__m128 va, vd;
	va = _mm_load_ps((const float*)&a);
#ifdef __SSE4_1__
	vd = _mm_dp_ps(va,va,0x77);
#else
	__m128 v0, v1;
	v0 = _mm_mul_ps(va,va);
	v1 = _mm_hadd_ps(v0,v0);
	vd = _mm_hadd_ps(v1,v1);
#endif
	return _mm_cvtss_f32(_mm_sqrt_ss(vd));
}
void cross( vect_t *o, vect_t a, vect_t b )
{
	__m128 ma, mb, t0, t1, t2, t3, to;
	ma = _mm_load_ps((const float*)&a);
	mb = _mm_load_ps((const float*)&b);
	t0 = _mm_shuffle_ps(ma,ma,0xc9);
	t1 = _mm_shuffle_ps(ma,ma,0xd2);
	t2 = _mm_shuffle_ps(mb,mb,0xd2);
	t3 = _mm_shuffle_ps(mb,mb,0xc9);
	to = _mm_sub_ps(_mm_mul_ps(t0,t2),_mm_mul_ps(t1,t3));
	_mm_store_ps((float*)o,to);
}
#else

// TODO

#endif

void vlerp( vect_t *o, vect_t a, vect_t b, float f )
{
	vect_t va, vb;
	vscale(&va,a,1.f-f);
	vscale(&vb,b,f);
	vadd(o,va,vb);
}
void normalize( vect_t *a )
{
	float s = vsize(*a);
	vscale(a,*a,s);
}
void reflect( vect_t *o, vect_t a, vect_t b )
{
	float dotp = 2.f*dot(a,b);
	vect_t c;
	vscale(&c,b,dotp);
	vsub(o,a,c);
}
static const mat_t ident =
{
	{{1,0,0,0},
	{0,1,0,0},
	{0,0,1,0},
	{0,0,0,1}}
};
void mident( mat_t *o )
{
	/* save time by just memcpy-ing a pregenerated one */
	memcpy(o,&ident,sizeof(mat_t));
}
#define PI 3.14159265359f
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
	float theta = (angle/180.f)*PI;
	vect_t n = {0.f,0.f,0.f,0.f};
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
void translate( mat_t *o, vect_t offset )
{
	o->c[0][0] = 1.f;
	o->c[0][1] = 0.f;
	o->c[0][2] = 0.f;
	o->c[0][3] = 0.f;
	o->c[1][0] = 0.f;
	o->c[1][1] = 1.f;
	o->c[1][2] = 0.f;
	o->c[1][3] = 0.f;
	o->c[2][0] = 0.f;
	o->c[2][1] = 0.f;
	o->c[2][2] = 1.f;
	o->c[2][3] = 0.f;
	o->c[3][0] = offset.x;
	o->c[3][1] = offset.y;
	o->c[3][2] = offset.z;
	o->c[3][3] = 1.f;
}
#define saturate(x) (((x)>0)?((x)>1)?1:(x):0)
#define swap(t,x,y) {t tmp=x;x=y;y=tmp;}
#define abs(x) (((x)>0)?(x):-(x))
#define min(a,b) (((a)>(b))?(b):(a))
#define max(a,b) (((a)>(b))?(a):(b))
#define clamp(a,b,c) (((a)>(b))?((a)>(c))?(c):(a):(b))
#define lerp(a,b,f) ((a)*(1.f-(f))+(b)*(f))

#define AGL_DT_LEQUAL  0
#define AGL_DT_GREATER 1
#define AGL_CULL_BACK  0
#define AGL_CULL_FRONT 1
#define AGL_PUTP_COLOR 1
#define AGL_PUTP_DEPTH 2

void agl_putpixel( buffer_t *screen, px_t p, vect_t c, int param )
{
	// sanity check
	if ( (p.x < 0) || (p.x >= screen->width)
		|| (p.y < 0) || (p.y >= screen->height) )
		return;
	int coord = p.x+p.y*screen->width;
	if ( param&AGL_PUTP_DEPTH ) screen->depth[coord] = p.d;
	if ( param&AGL_PUTP_COLOR )
	{
		screen->color[coord].r = saturate(c.x)*255.f;
		screen->color[coord].g = saturate(c.y)*255.f;
		screen->color[coord].b = saturate(c.z)*255.f;
		screen->color[coord].a = saturate(c.w)*255.f;
	}
}

void agl_drawline( buffer_t *screen, int x0, int y0, int x1, int y1, vect_t c )
{
	int steep = 0;
	if ( abs(x0-x1) < abs(y0-y1) )
	{
		swap(int,x0,y0);
		swap(int,x1,y1);
		steep = 1;
	}
	if ( x0 > x1 )
	{
		swap(int,x0,x1);
		swap(int,y0,y1);
	}
	int dx = x1-x0;
	int dy = y1-y0;
	int der = abs(dy)*2;
	int er = 0;
	int y = y0;
	for ( int x=x0; x<=x1; x++ )
	{
		px_t p = {x,y,0.f};
		if ( steep ) swap(int,p.x,p.y);
		agl_putpixel(screen,p,c,AGL_PUTP_COLOR);
		er += der;
		if ( er > dx )
		{
			y += (y1>y0)?1:-1;
			er -= dx*2;
		}
	}
}

void agl_sampletex( vect_t *o, coord_t c, texture_t *t )
{
	int cx = (c.s-floorf(c.s))*t->width, cy = (c.t-floorf(c.t))*t->height;
	o->x = t->data[cx+cy*t->width].r/255.f;
	o->y = t->data[cx+cy*t->width].g/255.f;
	o->z = t->data[cx+cy*t->width].b/255.f;
	o->w = t->data[cx+cy*t->width].a/255.f;
}

#define afn(a,b,c) ((c.x-a.x)*(b.y-a.y)-(c.y-a.y)*(b.x-a.x))

static void agl_int_filltriangle( buffer_t *screen, px_t p[3], coord_t tc[3],
	vect_t c[3], vect_t vp[3], vect_t vn[3], texture_t *tex,
	int param, prog_t *prog )
{
	px_t minb, maxb;
	maxb.x = clamp(max(p[0].x,max(p[1].x,p[2].x)),0,screen->width);
	maxb.y = clamp(max(p[0].y,max(p[1].y,p[2].y)),0,screen->height);
	minb.x = clamp(min(p[0].x,min(p[1].x,p[2].x)),0,screen->width);
	minb.y = clamp(min(p[0].y,min(p[1].y,p[2].y)),0,screen->height);
	if ( (maxb.x == minb.x) || (maxb.y == minb.y) ) return;
	for ( int i=0; i<3; i++ )
	{
		float id = 1.f/p[i].d;
		tc[i].s *= id;
		tc[i].t *= id;
		vscale(&c[i],c[i],id);
		vscale(&vp[i],vp[i],id);
		vscale(&vn[i],vn[i],id);
		p[i].d = id;
	}
	float area = afn(p[0],p[1],p[2]);
	frag_t frag;
	frag.screen = screen;
	frag.texture = tex;
	for ( int y=minb.y; y<maxb.y; y++ )
	{
		for ( int x=minb.x; x<maxb.x; x++ )
		{
			px_t px = {x+0.5f,y+0.5f,0};
			vect_t q =
			{
				afn(p[1],p[2],px)/area,
				afn(p[2],p[0],px)/area,
				afn(p[0],p[1],px)/area,
				0.f
			};
			if ( (q.x<0.f) || (q.y<0.f) || (q.z<0.f) ) continue;
			int coord = x+y*screen->width;
			float dep = screen->depth[coord];
			float pd = 1.f/(q.x*p[0].d+q.y*p[1].d+q.z*p[2].d);
			if ( (param&AGL_DT_LEQUAL) && (pd > dep) ) continue;
			if ( (param&AGL_DT_GREATER) && (pd <= dep) ) continue;
			coord_t tx =
			{
				pd*(q.x*tc[0].s+q.y*tc[1].s+q.z*tc[2].s),
				pd*(q.x*tc[0].t+q.y*tc[1].t+q.z*tc[2].t)
			};
			vect_t pc =
			{
				pd*(q.x*c[0].x+q.y*c[1].x+q.z*c[2].x),
				pd*(q.x*c[0].y+q.y*c[1].y+q.z*c[2].y),
				pd*(q.x*c[0].z+q.y*c[1].z+q.z*c[2].z),
				pd*(q.x*c[0].w+q.y*c[1].w+q.z*c[2].w)
			};
			vect_t fp =
			{
				pd*(q.x*vp[0].x+q.y*vp[1].x+q.z*vp[2].x),
				pd*(q.x*vp[0].y+q.y*vp[1].y+q.z*vp[2].y),
				pd*(q.x*vp[0].z+q.y*vp[1].z+q.z*vp[2].z),
				pd*(q.x*vp[0].w+q.y*vp[1].w+q.z*vp[2].w)
			};
			vect_t fn =
			{
				pd*(q.x*vn[0].x+q.y*vn[1].x+q.z*vn[2].x),
				pd*(q.x*vn[0].y+q.y*vn[1].y+q.z*vn[2].y),
				pd*(q.x*vn[0].z+q.y*vn[1].z+q.z*vn[2].z),
				pd*(q.x*vn[0].w+q.y*vn[1].w+q.z*vn[2].w)
			};
			px.x = x;
			px.y = y;
			px.d = pd;
			frag.fragdata = px;
			frag.position = fp;
			frag.normal = fn;
			frag.txcoord = tx;
			frag.color = pc;
			prog(frag);
		}
	}
}

static void agl_int_drawclippedtriangle( buffer_t *screen, tri_t t, int param,
	prog_t *prog )
{
	px_t pts[3] =
	{
		{(1.f+t.v[0].x/t.v[0].z)*0.5f*screen->width,
			(1.f+t.v[0].y/t.v[0].z)*0.5f*screen->height,
			t.v[0].z},
		{(1.f+t.v[1].x/t.v[1].z)*0.5f*screen->width,
			(1.f+t.v[1].y/t.v[1].z)*0.5f*screen->height,
			t.v[1].z},
		{(1.f+t.v[2].x/t.v[2].z)*0.5f*screen->width,
			(1.f+t.v[2].y/t.v[2].z)*0.5f*screen->height,
			t.v[2].z}
	};
	vect_t facet, ab, ac;
	ab.x = pts[1].x-pts[0].x;
	ab.y = pts[1].y-pts[0].y;
	ab.z = pts[1].d-pts[0].d;
	ac.x = pts[2].x-pts[0].x;
	ac.y = pts[2].y-pts[0].y;
	ac.z = pts[2].d-pts[0].d;
	cross(&facet,ab,ac);
	if ( (param&AGL_CULL_BACK) && (facet.z < 0.f) ) return;
	if ( (param&AGL_CULL_FRONT) && (facet.z > 0.f) ) return;
	vect_t cols[3] = {t.c[0],t.c[1],t.c[2]};
	coord_t coords[3] = {t.t[0],t.t[1],t.t[2]};
	vect_t pos[3] = {t.w[0],t.w[1],t.w[2]};
	vect_t norm[3] = {t.n[0],t.n[1],t.n[2]};
	agl_int_filltriangle(screen,pts,coords,cols,pos,norm,t.m,param,prog);
}

void agl_drawtriangle( buffer_t *screen, tri_t t, int param, prog_t *prog )
{
	t.v[0].z *= -1;
	t.v[1].z *= -1;
	t.v[2].z *= -1;
	if ( (t.v[0].z < screen->znear) && (t.v[1].z < screen->znear)
		&& (t.v[2].z < screen->znear) ) return;
	if ( (t.v[0].z > screen->zfar) && (t.v[1].z > screen->zfar)
		&& (t.v[2].z > screen->zfar) ) return;
	int ff = 0, fb = 0, fc = 0;
	for ( int i=0; i<3; i++ )
	{
		if ( t.v[i].z < screen->znear )
		{
			fb = i;
			fc++;
		}
		else ff = i;
	}
	if ( fc == 2 )
	{
		tri_t newt;
		vect_t p1, p2;
		float t1, t2;
		int a = (ff+1)%3, b = (ff+2)%3, c = ff;
		vsub(&p1,t.v[a],t.v[c]);
		vsub(&p2,t.v[b],t.v[c]);
		t1 = (screen->znear-t.v[c].z)/p1.z;
		t2 = (screen->znear-t.v[c].z)/p2.z;
		newt.m = t.m;
		newt.v[0].x = t.v[c].x+p1.x*t1;
		newt.v[0].y = t.v[c].y+p1.y*t1;
		newt.v[0].z = screen->znear;
		newt.v[0].w = 1.f;
		vlerp(&newt.n[0],t.n[c],t.n[a],t1);
		vlerp(&newt.w[0],t.w[c],t.w[a],t1);
		vlerp(&newt.c[0],t.c[c],t.c[a],t1);
		newt.t[0].s = lerp(t.t[c].s,t.t[a].s,t1);
		newt.t[0].t = lerp(t.t[c].t,t.t[a].t,t1);
		newt.v[1].x = t.v[c].x+p2.x*t2;
		newt.v[1].y = t.v[c].y+p2.y*t2;
		newt.v[1].z = screen->znear;
		newt.v[1].w = 1.f;
		vlerp(&newt.n[1],t.n[c],t.n[b],t2);
		vlerp(&newt.w[1],t.w[c],t.w[b],t2);
		vlerp(&newt.c[1],t.c[c],t.c[b],t2);
		newt.t[1].s = lerp(t.t[c].s,t.t[b].s,t2);
		newt.t[1].t = lerp(t.t[c].t,t.t[b].t,t2);
		newt.v[2] = t.v[c];
		newt.w[2] = t.w[c];
		newt.n[2] = t.n[c];
		newt.c[2] = t.c[c];
		newt.t[2] = t.t[c];
		agl_int_drawclippedtriangle(screen,newt,param,prog);
	}
	else if ( fc == 1 )
	{
		tri_t newt1, newt2;
		vect_t p1, p2;
		float t1, t2;
		int a = (fb+1)%3, b = (fb+2)%3, c = fb;
		vsub(&p1,t.v[c],t.v[a]);
		vsub(&p2,t.v[c],t.v[b]);
		t1 = (screen->znear-t.v[a].z)/p1.z;
		t2 = (screen->znear-t.v[b].z)/p2.z;
		newt1.m = t.m;
		newt1.v[0] = t.v[a];
		newt1.w[0] = t.w[a];
		newt1.n[0] = t.n[a];
		newt1.c[0] = t.c[a];
		newt1.t[0] = t.t[a];
		newt1.v[1] = t.v[b];
		newt1.w[1] = t.w[b];
		newt1.n[1] = t.n[b];
		newt1.c[1] = t.c[b];
		newt1.t[1] = t.t[b];
		newt1.v[2].x = t.v[a].x+p1.x*t1;
		newt1.v[2].y = t.v[a].y+p1.y*t1;
		newt1.v[2].z = screen->znear;
		newt1.v[2].w = 1.f;
		vlerp(&newt1.n[2],t.n[a],t.n[c],t1);
		vlerp(&newt1.w[2],t.w[a],t.w[c],t1);
		vlerp(&newt1.c[2],t.c[a],t.c[c],t1);
		newt1.t[2].s = lerp(t.t[a].s,t.t[c].s,t1);
		newt1.t[2].t = lerp(t.t[a].t,t.t[c].t,t1);
		agl_int_drawclippedtriangle(screen,newt1,param,prog);
		newt2.m = t.m;
		newt2.v[0].x = t.v[a].x+p1.x*t1;
		newt2.v[0].y = t.v[a].y+p1.y*t1;
		newt2.v[0].z = screen->znear;
		newt2.v[0].w = 1.f;
		vlerp(&newt2.n[0],t.n[a],t.n[c],t1);
		vlerp(&newt2.w[0],t.w[a],t.w[c],t1);
		vlerp(&newt2.c[0],t.c[a],t.c[c],t1);
		newt2.t[0].s = lerp(t.t[a].s,t.t[c].s,t1);
		newt2.t[0].t = lerp(t.t[a].t,t.t[c].t,t1);
		newt2.v[1] = t.v[b];
		newt2.w[1] = t.w[b];
		newt2.n[1] = t.n[b];
		newt2.c[1] = t.c[b];
		newt2.t[1] = t.t[b];
		newt2.v[2].x = t.v[b].x+p2.x*t2;
		newt2.v[2].y = t.v[b].y+p2.y*t2;
		newt2.v[2].z = screen->znear;
		newt2.v[2].w = 1.f;
		vlerp(&newt2.n[2],t.n[b],t.n[c],t2);
		vlerp(&newt2.w[2],t.w[b],t.w[c],t2);
		vlerp(&newt2.c[2],t.c[b],t.c[c],t2);
		newt2.t[2].s = lerp(t.t[b].s,t.t[c].s,t2);
		newt2.t[2].t = lerp(t.t[b].t,t.t[c].t,t2);
		agl_int_drawclippedtriangle(screen,newt2,param,prog);
	}
	else agl_int_drawclippedtriangle(screen,t,param,prog);
}

void agl_clearcolor( buffer_t *screen, int r, int g, int b )
{
	uint32_t ival = 0xFF000000|r|(g<<8)|(b<<16);
	uint32_t *iscr = (uint32_t*)(screen->color);
	for ( int i=0; i<screen->width*screen->height; i++ )
		iscr[i] = ival;
}

void agl_cleardepth( buffer_t *screen )
{
	for ( int i=0; i<screen->width*screen->height; i++ )
		screen->depth[i] = screen->zfar;
}
