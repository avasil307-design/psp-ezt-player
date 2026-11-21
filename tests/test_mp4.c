#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp4.h"

typedef struct { FILE *f; } file_ctx;
static int rd(void *u, uint64_t off, void *dst, size_t n)
{
    file_ctx *c=(file_ctx*)u;
    if(fseek(c->f,(long)off,SEEK_SET)!=0) return -1;
    return fread(dst,1,n,c->f)==n?0:-1;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    file_ctx c; c.f=fopen(argv[1],"rb"); assert(c.f);
    assert(fseek(c.f,0,SEEK_END)==0); long n=ftell(c.f); assert(n>0);
    ez_mp4_io io={&c,(uint64_t)n,rd};
    ez_mp4_video v;
    assert(ez_mp4_open(&io,&v)==EZ_MP4_OK);
    assert(v.width==320&&v.height==240);
    assert(v.timescale==1000&&v.duration==2000);
    assert(v.sample_count==2&&v.avc_profile==0x42&&v.avc_level==0x1e);
    assert(v.nal_length_size==4&&v.sps_size==4&&v.pps_size==2);
    assert(v.samples[0].keyframe&&v.samples[0].duration==1000);
    assert(!v.samples[1].keyframe&&v.samples[1].size==6);
    uint8_t out[32];
    int got=ez_mp4_read_annexb(&io,&v,0,out,sizeof out);
    static const uint8_t expect[]={0,0,0,1,0x65,0xaa,0xbb};
    assert(got==(int)sizeof expect&&memcmp(out,expect,sizeof expect)==0);
    assert(ez_mp4_read_annexb(&io,&v,1,out,4)==EZ_MP4_ERR_RANGE);
    ez_mp4_close(&v);
    fclose(c.f);
    puts("MP4 demux tests passed");
    return 0;
}
