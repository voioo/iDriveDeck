#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "surface_draw.c"

static void row_title(struct canvas c,const char* expected) {
    const size_t bytes=(size_t)c.stride*c.height;
    unsigned char* reference=malloc(bytes);assert(reference);
    memcpy(reference,c.buffer,bytes);
    struct canvas control=c;control.buffer=reference;
    card(control,24,41,592,34,false);
    text(control,36,41,expected,bright,16);
    const int top=41*c.height/240,bottom=57*c.height/240;
    for (int y=top;y<bottom;++y)
        assert(!memcmp(c.buffer+y*c.stride+36*c.width/640*4,
                       reference+y*c.stride+36*c.width/640*4,(size_t)(558*c.width/640)*4));
    free(reference);
}

int main(void) {
    char title[96],detail[192];
    full_fault_row("100213  status 09 mask 08",0,1,title,sizeof title,detail,sizeof detail);
    assert(!strcmp(title,"100213  Confirmed") && !strcmp(detail,"Status 09 / supported 08"));
    full_fault_row("100213  status 01",0,1,title,sizeof title,detail,sizeof detail);
    assert(!strcmp(title,"100213  Raw") && strstr(detail,"mask unavailable"));
    full_fault_row("100213  status 00 mask FF",0,0,title,sizeof title,detail,sizeof detail);
    assert(!strcmp(title,"100213  Unknown") && !strcmp(detail,dme_code_description(0x100213)));
    full_fault_row("OBD03 P0133",7,0,title,sizeof title,detail,sizeof detail);
    assert(!strcmp(title,"OBD03 P0133") && !detail[0]);
    full_fault_row("000001  status 01 mask FF",99,0,title,sizeof title,detail,sizeof detail);
    assert(!strcmp(title,"Diagnostic result unavailable") && !detail[0]);
    const unsigned panels[][2]={{800,480},{960,480},{1280,480},{1440,540},{1920,720}};
    for (unsigned p=0;p<COUNT(panels);++p) {
        const unsigned width=panels[p][0],height=panels[p][1];
        const size_t bytes=(size_t)width*height*4;
        unsigned char* allocation=malloc(bytes+64);assert(allocation);memset(allocation,0xa5,bytes+64);
        struct canvas c={allocation+32,(int)width,(int)height,(int)width*4,canvas_rgba};
        struct surface_state state;memset(&state,0,sizeof state);state.magic=surface_magic;state.page=surface_codes;
        strcpy(state.values[0],"100213  status 09 mask 08");
        for (unsigned raw=0;raw<2;++raw) {
            state.boost_style=(uint8_t)raw;surface_draw(c,&state);row_title(c,"100213  Confirmed");
        }
        const unsigned services[]={3,7,10};
        const char* records[]={"OBD03 P0133","OBD07 C1234","OBD0A U1016"};
        const char* labels[]={"P0133  Confirmed","C1234  Pending","U1016  Permanent"};
        for (unsigned i=0;i<3;++i) {
            state.launch_mode=services[i];strcpy(state.values[0],records[i]);
            surface_draw(c,&state);row_title(c,labels[i]);
        }
        state.boost_style=2;surface_draw(c,&state);row_title(c,"Diagnostic result unavailable");
        for (unsigned i=0;i<2;++i) { state.selection=i ? 234 : 236;surface_draw(c,&state); }
        for (unsigned i=0;i<32;++i) assert(allocation[i]==0xa5 && allocation[bytes+32+i]==0xa5);
        free(allocation);
    }
    puts("Full-screen faults: actual C titles on five panels, masked/legacy states, raw/OBD dictionary exclusion, malformed metadata and confirmation bounds passed.");
}
