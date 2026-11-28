#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "surface_draw.c"

int main(void) {
    assert(COUNT(surface_titles)==9 && !strcmp(surface_titles[surface_dpf],"DPFDeck"));
    const unsigned panels[][2]={{800,480},{960,480},{1280,480},{1440,540},{1920,720}};
    for (unsigned p=0;p<COUNT(panels);++p) {
        const unsigned width=panels[p][0],height=panels[p][1];const size_t bytes=(size_t)width*height*4;
        unsigned char* allocation=malloc(bytes+64);assert(allocation);memset(allocation,0xa5,bytes+64);
        struct canvas c={allocation+32,(int)width,(int)height,(int)width*4,canvas_rgba};
        struct surface_state state;memset(&state,0,sizeof state);state.magic=surface_magic;state.page=surface_dpf;
        strcpy(state.values[7],"Bank 2 | psi, F");strcpy(state.values[6],"Regeneration: Not in progress");
        strcpy(state.status,"Some DPF readings unavailable");
        for (unsigned current=0;current<6;++current) {
            for (unsigned i=0;i<6;++i) {
                state.boost_signal[i]=(uint8_t)current;
                strcpy(state.values[i],i<3 ? "6553.5 mbar" : "1800.0 C");
            }
            surface_draw(c,&state);
            const char* reading=current==DPF_VALID ? state.values[0] : current<4 ?
                (const char* const[]){"Unknown","Unsupported","Unavailable","Stale"}[current] : "--";
            unsigned char* reference=malloc(bytes);assert(reference);memcpy(reference,c.buffer,bytes);
            struct canvas control=c;control.buffer=reference;
            card(control,24,40,192,65,false);text(control,34,45,"Pressure drop",muted,16);
            text(control,34,70,reading,current==DPF_VALID ? bright : muted,boost_text_size(reading,24,172));
            for (unsigned y=40*height/240;y<105*height/240;++y)
                assert(!memcmp(c.buffer+y*c.stride+24*width/640*4,reference+y*c.stride+24*width/640*4,192*width/640*4));
            free(reference);
        }
        for (unsigned i=0;i<32;++i) assert(allocation[i]==0xa5 && allocation[bytes+32+i]==0xa5);
        free(allocation);
    }
    puts("Full-screen DPF: actual C renderer on five panels, all measurement states, invalid-state fail-closed values and layout buffer bounds passed.");
}
