#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "surface_draw.c"
#include "host_surface.c"

static unsigned pixel(struct canvas c,unsigned x,unsigned y) {
    const unsigned char* p=c.buffer+((y*(unsigned)c.height/240)*(unsigned)c.width+x*(unsigned)c.width/640)*4;
    return (unsigned)p[0]<<16 | (unsigned)p[1]<<8 | p[2];
}

int main(void) {
    assert(COUNT(boost_names)==58);
    for (unsigned signal=31;signal<58;++signal) {
        assert(word_width(boost_names[signal][0],boost_text_size(boost_names[signal][0],17,102))<=102);
        assert(word_width(boost_names[signal][1],boost_text_size(boost_names[signal][1],17,61))<=61);
    }
    const char* values[]={"655350.0 kPa","327.675 L/h","6513.5 C","100.0%"};
    for (unsigned i=0;i<COUNT(values);++i)
        assert(word_width(values[i],boost_text_size(values[i],24,102))<=102);
    const unsigned panels[][2]={{800,480},{960,480},{1280,480},{1440,540},{1920,720}};
    for (unsigned panel_index=0;panel_index<COUNT(panels);++panel_index) {
        const unsigned width=panels[panel_index][0],height=panels[panel_index][1];
        const size_t size=(size_t)width*height*4;
        unsigned char* allocation=malloc(size+64);assert(allocation);memset(allocation,0xa5,size+64);
        struct canvas canvas={allocation+32,(int)width,(int)height,(int)width*4,canvas_rgba};
        struct surface_state state;memset(&state,0,sizeof state);state.magic=surface_magic;state.page=surface_boost;
        for (unsigned signal=0;signal<58;++signal) {
            for (unsigned i=0;i<9;++i) {
                state.boost_signal[i]=(uint8_t)signal;state.boost_level[i]=1000;
                snprintf(state.values[i],sizeof state.values[i],"%s",values[signal%COUNT(values)]);
            }
            for (unsigned style=0;style<2;++style) { state.boost_style=(uint8_t)style;surface_draw(canvas,&state); }
        }
        state.boost_style=0;state.boost_signal[0]=255;state.boost_level[0]=1000;
        surface_draw(canvas,&state);assert(pixel(canvas,42,105)==frame);
        state.boost_signal[0]=BD_REAGENT_LEVEL;state.boost_level[0]=0;strcpy(state.values[0],"0.0%");
        surface_draw(canvas,&state);assert(pixel(canvas,42,105)==frame);
        const unsigned maxima[]={1000,1600,2000,2500},levels[]={1000,625,500,400};
        const char* const captions[3][4]={
            {"Rail 0-14504","Rail 0-23206","Rail 0-29008","Rail 0-36259"},
            {"Rail 0-1000","Rail 0-1600","Rail 0-2000","Rail 0-2500"},
            {"Rail 0-100000","Rail 0-160000","Rail 0-200000","Rail 0-250000"}};
        for (unsigned units=0;units<3;++units) for (unsigned choice=0;choice<4;++choice) {
            char caption[40];
            assert(full_boost_rail_caption(caption,sizeof caption,maxima[choice],units));
            assert(!strcmp(caption,captions[units][choice]));
            assert(word_width(caption,boost_text_size(caption,17,61))<=61);
            state.shift_target=maxima[choice];state.launch_mode=units;
            for (unsigned slot=0;slot<9;++slot) {
                state.boost_signal[slot]=BD_RAIL;state.boost_level[slot]=(uint16_t)levels[choice];
                strcpy(state.values[slot],units==0 ? "14504 psi" : units==1 ? "1000 bar" : "100000 kPa");
            }
            struct surface_animation animation={0,0,0,0};
            struct surface_state animated;
            overlay_frame(&animation,&state,&animated,1000);
            assert(animated.launch_mode==units && animated.shift_target==maxima[choice]);
            for (unsigned style=0;style<2;++style) {
                animated.boost_style=(uint8_t)style;surface_draw(canvas,&animated);
                if (!style) {
                    assert((pixel(canvas,90,105)!=frame)==(choice<2));
                    assert((pixel(canvas,575,125)!=frame)==(choice<3));
                }
            }
        }
        state.boost_style=0;state.launch_mode=3;surface_draw(canvas,&state);
        assert(pixel(canvas,42,105)==frame);
        state.launch_mode=1;state.shift_target=42;surface_draw(canvas,&state);
        assert(pixel(canvas,42,105)==frame);
        for (unsigned i=0;i<32;++i) assert(allocation[i]==0xa5 && allocation[size+32+i]==0xa5);
        free(allocation);
    }
    puts("Full-screen Boost: all58 captions, extended value bounds, both styles/five panels, unknown slots and validzero passed.");
}
