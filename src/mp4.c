/* Minimal bounds-checked ISO BMFF demuxer for AVC-in-MP4 sample tables. */
#include <stdlib.h>
#include <string.h>
#include "mp4.h"

#define MAX_SAMPLES 250000u
#define MAX_CHUNKS 250000u
#define MAX_RUNS 16384u
#define MAX_STSC 4096u
#define AVC_SAMPLE_MAX (2u * 1024u * 1024u)

typedef struct { uint32_t count, delta; } time_run;
typedef struct { uint32_t first, samples_per_chunk; } stsc_entry;
typedef struct {
    uint32_t size;
    uint64_t offset;
} chunk_entry;
typedef struct {
    const ez_mp4_io *io;
    uint64_t moov_end;
    uint32_t timescale, duration;
    uint32_t width, height;
    uint32_t *sizes; uint32_t size_count;
    time_run *times; uint32_t time_count;
    stsc_entry *stsc; uint32_t stsc_count;
    uint64_t *chunks; uint32_t chunk_count;
    uint32_t *sync; uint32_t sync_count; int has_sync;
    uint8_t profile, level, nal_len;
    uint8_t sps[64], pps[64], sps_n, pps_n;
    int is_video, avc;
} track_parse;

typedef struct { uint64_t pos, end; uint32_t type; } atom;

static uint32_t be32(const uint8_t *p)
{ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static uint64_t be64(const uint8_t *p)
{ return ((uint64_t)be32(p)<<32)|be32(p+4); }
static uint32_t fourcc(const char s[4])
{ return ((uint32_t)(uint8_t)s[0]<<24)|((uint32_t)(uint8_t)s[1]<<16)|((uint32_t)(uint8_t)s[2]<<8)|(uint8_t)s[3]; }
static int read_bytes(const ez_mp4_io *io, uint64_t off, void *p, size_t n)
{ return off <= io->size && n <= io->size - off && io->read_at(io->user, off, p, n) == 0; }
static int read_atom(const ez_mp4_io *io, uint64_t pos, uint64_t end, atom *a)
{
    uint8_t h[16];
    if (pos > end || end - pos < 8 || !read_bytes(io, pos, h, 8)) return 0;
    uint64_t len = be32(h); uint32_t type = be32(h+4); uint64_t head = 8;
    if (len == 1) {
        if (end-pos < 16 || !read_bytes(io, pos+8, h+8, 8)) return 0;
        len = be64(h+8); head = 16;
    } else if (len == 0) len = end-pos;
    if (len < head || len > end-pos) return 0;
    a->pos = pos + head; a->end = pos + len; a->type = type;
    return 1;
}
static int eq(uint32_t a, const char *b) { return a == fourcc(b); }
static int child_atom(const ez_mp4_io *io, uint64_t *pos, uint64_t end, atom *a)
{
    if (*pos == end) return 0;
    if (!read_atom(io, *pos, end, a)) { *pos = end; return -1; }
    *pos = a->end;
    return 1;
}
static int alloc_table(void **p, uint32_t n, size_t elem)
{
    if (!n || (size_t)n > SIZE_MAX / elem) return 0;
    *p = malloc((size_t)n * elem);
    return *p != 0;
}
static int parse_avcc(track_parse *t, const ez_mp4_io *io, atom a)
{
    uint8_t buf[1024]; size_t len = (size_t)(a.end-a.pos);
    if (len < 7 || len > sizeof buf || !read_bytes(io,a.pos,buf,len) || buf[0] != 1) return 0;
    t->profile=buf[1]; t->level=buf[3]; t->nal_len=(uint8_t)((buf[4]&3)+1);
    size_t p=6; unsigned ns=buf[5]&31;
    for (unsigned i=0;i<ns;i++) {
        if (p+2>len) return 0;
        unsigned n=(buf[p]<<8)|buf[p+1]; p+=2;
        if (!n || p+n>len) return 0;
        if (!t->sps_n && n<=sizeof t->sps) { memcpy(t->sps,buf+p,n); t->sps_n=(uint8_t)n; }
        p+=n;
    }
    if (p>=len) return 0;
    unsigned np=buf[p++];
    for (unsigned i=0;i<np;i++) {
        if (p+2>len) return 0;
        unsigned n=(buf[p]<<8)|buf[p+1]; p+=2;
        if (!n || p+n>len) return 0;
        if (!t->pps_n && n<=sizeof t->pps) { memcpy(t->pps,buf+p,n); t->pps_n=(uint8_t)n; }
        p+=n;
    }
    t->avc=(t->sps_n && t->pps_n && t->nal_len>=1 && t->nal_len<=4);
    return t->avc;
}
static int parse_stsd(track_parse *t, atom a)
{
    uint8_t full[8];
    if (a.end-a.pos<8 || !read_bytes(t->io,a.pos,full,8)) return 0;
    uint32_t count=be32(full+4); uint64_t p=a.pos+8;
    for(uint32_t i=0;i<count && p<a.end;i++) {
        atom ent; if(!read_atom(t->io,p,a.end,&ent)) return 0;
        if ((eq(ent.type,"avc1")||eq(ent.type,"avc3")) && ent.end-ent.pos>=78) {
            uint8_t v[28];
            if(!read_bytes(t->io,ent.pos,v,sizeof v)) return 0;
            t->width=(v[24]<<8)|v[25]; t->height=(v[26]<<8)|v[27];
            uint64_t cp=ent.pos+78;
            while(cp<ent.end) {
                atom sub; if(!read_atom(t->io,cp,ent.end,&sub)) return 0;
                if(eq(sub.type,"avcC")) { if(!parse_avcc(t,t->io,sub)) return 0; break; }
                cp=sub.end;
            }
        }
        p=ent.end;
    }
    return 1;
}
static int parse_stsz(track_parse *t, atom a)
{
    uint8_t h[12]; if(a.end-a.pos<12 || !read_bytes(t->io,a.pos,h,12)) return 0;
    uint32_t fixed=be32(h+4), count=be32(h+8);
    if(!count || count>MAX_SAMPLES) return 0;
    if(!alloc_table((void**)&t->sizes,count,sizeof(uint32_t))) return -1;
    t->size_count=count;
    if(fixed) { for(uint32_t i=0;i<count;i++) t->sizes[i]=fixed; return 1; }
    if(a.end-a.pos<12+(uint64_t)count*4) return 0;
    uint8_t b[4]; uint64_t p=a.pos+12;
    for(uint32_t i=0;i<count;i++,p+=4) { if(!read_bytes(t->io,p,b,4)) return 0; t->sizes[i]=be32(b); if(!t->sizes[i]||t->sizes[i]>AVC_SAMPLE_MAX) return 0; }
    return 1;
}
static int parse_stts(track_parse *t, atom a)
{
    uint8_t h[8]; if(a.end-a.pos<8||!read_bytes(t->io,a.pos,h,8))return 0;
    uint32_t n=be32(h+4); if(!n||n>MAX_RUNS||a.end-a.pos<8+(uint64_t)n*8)return 0;
    if(!alloc_table((void**)&t->times,n,sizeof(time_run)))return -1;
    t->time_count=n; uint64_t p=a.pos+8; uint8_t b[8];
    for(uint32_t i=0;i<n;i++,p+=8){if(!read_bytes(t->io,p,b,8))return 0;t->times[i]=(time_run){be32(b),be32(b+4)}; if(!t->times[i].count||!t->times[i].delta)return 0;}
    return 1;
}
static int parse_stsc(track_parse *t, atom a)
{
    uint8_t h[8];if(a.end-a.pos<8||!read_bytes(t->io,a.pos,h,8))return 0;
    uint32_t n=be32(h+4);if(!n||n>MAX_STSC||a.end-a.pos<8+(uint64_t)n*12)return 0;
    if(!alloc_table((void**)&t->stsc,n,sizeof(stsc_entry))) return -1;
    t->stsc_count=n;
    uint64_t p=a.pos+8;uint8_t b[12];
    for(uint32_t i=0;i<n;i++,p+=12){if(!read_bytes(t->io,p,b,12))return 0;t->stsc[i]=(stsc_entry){be32(b),be32(b+4)};if(!t->stsc[i].first||!t->stsc[i].samples_per_chunk||(i==0&&t->stsc[i].first!=1)||(i&&t->stsc[i].first<=t->stsc[i-1].first))return 0;}
    return 1;
}
static int parse_offsets(track_parse *t, atom a, int wide)
{
    uint8_t h[8];if(a.end-a.pos<8||!read_bytes(t->io,a.pos,h,8))return 0;
    uint32_t n=be32(h+4), step=wide?8:4;if(!n||n>MAX_CHUNKS||a.end-a.pos<8+(uint64_t)n*step)return 0;
    if(!alloc_table((void**)&t->chunks,n,sizeof(uint64_t))) return -1;
    t->chunk_count=n;
    uint64_t p=a.pos+8;uint8_t b[8];
    for(uint32_t i=0;i<n;i++,p+=step){if(!read_bytes(t->io,p,b,step))return 0;t->chunks[i]=wide?be64(b):be32(b);if(t->chunks[i]>=t->io->size)return 0;}
    return 1;
}
static int parse_stss(track_parse *t, atom a)
{
    uint8_t h[8];if(a.end-a.pos<8||!read_bytes(t->io,a.pos,h,8))return 0;
    uint32_t n=be32(h+4);if(n>MAX_SAMPLES||a.end-a.pos<8+(uint64_t)n*4)return 0;
    if(n && !alloc_table((void**)&t->sync,n,sizeof(uint32_t))) return -1;
    t->sync_count=n;
    t->has_sync=1;
    uint64_t p=a.pos+8;uint8_t b[4];for(uint32_t i=0;i<n;i++,p+=4){if(!read_bytes(t->io,p,b,4))return 0;t->sync[i]=be32(b);if(!t->sync[i]||(i&&t->sync[i]<=t->sync[i-1]))return 0;}
    return 1;
}
static int parse_stbl(track_parse *t, atom a)
{
    uint64_t p=a.pos;atom q;int r;
    while((r=child_atom(t->io,&p,a.end,&q))>0){int z=1;
        if(eq(q.type,"stsd"))z=parse_stsd(t,q);else if(eq(q.type,"stsz"))z=parse_stsz(t,q);else if(eq(q.type,"stts"))z=parse_stts(t,q);else if(eq(q.type,"stsc"))z=parse_stsc(t,q);else if(eq(q.type,"stco"))z=parse_offsets(t,q,0);else if(eq(q.type,"co64"))z=parse_offsets(t,q,1);else if(eq(q.type,"stss"))z=parse_stss(t,q);
        if(z<=0)return z;
    }
    return r<0?0:1;
}
static int walk(track_parse *t, atom parent)
{
    uint64_t p=parent.pos;atom a;int r;
    while((r=child_atom(t->io,&p,parent.end,&a))>0){
        if(eq(a.type,"hdlr")){uint8_t b[12];if(a.end-a.pos<12||!read_bytes(t->io,a.pos,b,12))return 0;t->is_video=be32(b+8)==fourcc("vide");}
        else if(eq(a.type,"mdhd")){uint8_t b[32];if(a.end-a.pos<20||!read_bytes(t->io,a.pos,b,(a.end-a.pos<32)?(size_t)(a.end-a.pos):32))return 0;uint8_t ver=b[0];if(ver==1){if(a.end-a.pos<32)return 0;t->timescale=be32(b+20);t->duration=(uint32_t)(be64(b+24)>0xffffffffu?0xffffffffu:be64(b+24));}else{t->timescale=be32(b+12);t->duration=be32(b+16);}}
        else if(eq(a.type,"stbl")){int z=parse_stbl(t,a);if(z<=0)return z;}
        else if(eq(a.type,"trak")||eq(a.type,"mdia")||eq(a.type,"minf")){int z=walk(t,a);if(z<=0)return z;}
    }
    return r<0?0:1;
}
static void free_track(track_parse *t){free(t->sizes);free(t->times);free(t->stsc);free(t->chunks);free(t->sync);}
static int build_samples(track_parse *t, ez_mp4_video *out)
{
    if(!t->is_video||!t->avc||!t->timescale||!t->size_count||!t->time_count||!t->stsc_count||!t->chunk_count)return 0;
    uint64_t total=0;for(uint32_t i=0;i<t->time_count;i++)total+=t->times[i].count;
    if(total!=t->size_count)return 0;
    ez_mp4_sample *samples=calloc(t->size_count,sizeof(*samples));if(!samples)return -1;
    uint32_t si=0,ri=0,sc=0;uint64_t off=0;
    for(uint32_t ch=0;ch<t->chunk_count;ch++){
        uint32_t chunk1=ch+1;
        while(sc+1<t->stsc_count&&t->stsc[sc+1].first<=chunk1)sc++;
        if(chunk1<t->stsc[sc].first){free(samples);return 0;}
        off=t->chunks[ch];
        for(uint32_t k=0;k<t->stsc[sc].samples_per_chunk;k++){
            if(si>=t->size_count){free(samples);return 0;}
            uint32_t dur=0;
            while(ri<t->time_count&&t->times[ri].count==0)ri++;
            if(ri>=t->time_count){free(samples);return 0;}
            dur=t->times[ri].delta;t->times[ri].count--;
            if(off>t->io->size||t->sizes[si]>t->io->size-off){free(samples);return 0;}
            samples[si]=(ez_mp4_sample){off,t->sizes[si],dur,1};
            off+=t->sizes[si];si++;
        }
    }
    if(si!=t->size_count){free(samples);return 0;}
    if(t->has_sync){for(uint32_t i=0;i<t->size_count;i++)samples[i].keyframe=0;for(uint32_t i=0;i<t->sync_count;i++){uint32_t n=t->sync[i];if(n>t->size_count){free(samples);return 0;}samples[n-1].keyframe=1;}}
    else for(uint32_t i=0;i<t->size_count;i++)samples[i].keyframe=1;
    out->timescale=t->timescale;out->duration=t->duration;out->width=t->width;out->height=t->height;out->sample_count=t->size_count;out->samples=samples;
    out->avc_profile=t->profile;out->avc_level=t->level;out->nal_length_size=t->nal_len;memcpy(out->sps,t->sps,t->sps_n);memcpy(out->pps,t->pps,t->pps_n);out->sps_size=t->sps_n;out->pps_size=t->pps_n;
    return 1;
}
ez_mp4_result ez_mp4_open(const ez_mp4_io *io, ez_mp4_video *out)
{
    if(!io||!io->read_at||!out||io->size<8)return EZ_MP4_ERR_RANGE;
    memset(out,0,sizeof(*out));uint64_t p=0;atom top;int found=0;
    while(p<io->size){if(!read_atom(io,p,io->size,&top))return EZ_MP4_ERR_FORMAT;p=top.end;if(eq(top.type,"moov")){found=1;break;}}
    if(!found)return EZ_MP4_ERR_FORMAT;
    uint64_t q=top.pos;atom tr;int r;ez_mp4_result result=EZ_MP4_ERR_UNSUPPORTED;
    while((r=child_atom(io,&q,top.end,&tr))>0){if(!eq(tr.type,"trak"))continue;
        track_parse t;memset(&t,0,sizeof t);t.io=io;int z=walk(&t,tr);
        if(z<0){free_track(&t);return EZ_MP4_ERR_MEMORY;}if(!z){free_track(&t);continue;}
        int b=build_samples(&t,out);free_track(&t);
        if(b<0) return EZ_MP4_ERR_MEMORY;
        if(b>0){result=EZ_MP4_OK;break;}
    }
    if(r<0)return EZ_MP4_ERR_FORMAT;
    return result;
}
void ez_mp4_close(ez_mp4_video *v){if(v){free(v->samples);memset(v,0,sizeof(*v));}}
int ez_mp4_read_annexb(const ez_mp4_io *io,const ez_mp4_video *v,uint32_t ix,uint8_t *dst,size_t cap)
{
    if(!io||!v||!dst||ix>=v->sample_count)return EZ_MP4_ERR_RANGE;
    const ez_mp4_sample *s=&v->samples[ix];if(s->size>AVC_SAMPLE_MAX)return EZ_MP4_ERR_RANGE;
    uint8_t *src=malloc(s->size);if(!src)return EZ_MP4_ERR_MEMORY;
    if(!read_bytes(io,s->offset,src,s->size)){free(src);return EZ_MP4_ERR_IO;}
    size_t p=0,o=0;while(p<s->size){if(s->size-p<v->nal_length_size){free(src);return EZ_MP4_ERR_FORMAT;}
        uint32_t n=0;for(uint8_t k=0;k<v->nal_length_size;k++)n=(n<<8)|src[p++];
        if(!n||n>s->size-p){free(src);return EZ_MP4_ERR_FORMAT;}
        if(cap-o<4u+n){free(src);return EZ_MP4_ERR_RANGE;}
        dst[o++]=0;dst[o++]=0;dst[o++]=0;dst[o++]=1;memcpy(dst+o,src+p,n);o+=n;p+=n;
    }
    free(src);return (int)o;
}
