#ifndef MPDTAB5_VOLUME_GRID_H
#define MPDTAB5_VOLUME_GRID_H

#include "volume_scale_tables.h"

/* Stable persisted/BLE IDs: 16/64 squared curves, 2/5 integer percent grids. */
static inline int volume_scale_valid(int mode) { return mode==16||mode==64||mode==2||mode==5; }
static inline int volume_steps_normalize(int mode) { return volume_scale_valid(mode)?mode:64; }
static inline int volume_grid_count(int mode) { return mode==2?50:mode==5?20:volume_steps_normalize(mode); }
static inline const char *volume_scale_name(int mode) {
    return mode==16?"1/16 Squared Curve":mode==64?"1/64 Squared Curve":mode==2?"2% Integer":mode==5?"5% Integer":"Unknown";
}
static inline const float *volume_grid_table(int mode) {
    return mode==16?volume_scale_16:mode==2?volume_scale_2:mode==5?volume_scale_5:volume_scale_64;
}
static inline int volume_grid_nearest(float level,int mode) {
    const float *table=volume_grid_table(mode);const int count=volume_grid_count(mode);
    if(!(level>=0))return 0;
    if(level>=1)return count;
    int low=0,high=count;
    while(low<high){const int middle=(low+high)/2;if(table[middle]<level)low=middle+1;else high=middle;}
    return low>0&&level-table[low-1]<table[low]-level?low-1:low;
}
static inline float volume_grid_display(float level,int mode) {
    if(!(level>=0&&level<=1))return -1;
    return mode==16||mode==64?(float)volume_grid_nearest(level,mode)/volume_grid_count(mode):level;
}
static inline float volume_grid_step(float level,int delta,int mode) {
    mode=volume_steps_normalize(mode);const float *table=volume_grid_table(mode);const int count=volume_grid_count(mode);
    if(!(level>=0))level=0;
    if(level>1)level=1;
    int index=volume_grid_nearest(level,mode);
    /* Curved readback identifies its nearest logical lookup position. Integer
     * grids keep directional snapping for external off-grid percentages. */
    if(mode==2||mode==5){
        const float difference=level-table[index];
        if(delta>0&&difference<-.000001f)--index;
        else if(delta<0&&difference>.000001f)++index;
    }
    if(delta>count-index)index=count;
    else if(delta<-index)index=0;
    else index+=delta;
    return table[index];
}
static inline float volume_grid_snap(float level,int mode) {
    return volume_grid_table(mode)[volume_grid_nearest(level,mode)];
}

/* Keep the logical grid target separately from actual mixer feedback.
 * Only the first readback may establish a quantized target/echo pair. Later
 * external changes invalidate it, even when close to the old target. */
typedef struct {
    float target,echo;
    int steps,active,pending;
} volume_grid_cursor_t;
static inline int volume_grid_near(float a,float b,float tolerance)
{
    const float difference=a-b;
    return difference>=-tolerance&&difference<=tolerance;
}
static inline void volume_grid_requested(volume_grid_cursor_t *cursor,float target,int steps)
{
    cursor->target=target;cursor->steps=volume_steps_normalize(steps);
    cursor->active=cursor->pending=1;
}
static inline void volume_grid_observe(volume_grid_cursor_t *cursor,float actual)
{
    if(!cursor->active)return;
    if(!(actual>=0&&actual<=1)) { cursor->active=0;return; }
    if(cursor->pending){
        cursor->pending=0;
        if(volume_grid_nearest(actual,cursor->steps)!=volume_grid_nearest(cursor->target,cursor->steps))cursor->active=0;
        else cursor->echo=actual;
    }else if(!volume_grid_near(actual,cursor->echo,0.000001f))cursor->active=0;
}
static inline float volume_grid_next(const volume_grid_cursor_t *cursor,float actual,int delta,int steps)
{
    const int anchored=cursor->active&&!cursor->pending&&
        cursor->steps==volume_steps_normalize(steps)&&volume_grid_near(actual,cursor->echo,0.000001f);
    return volume_grid_step(anchored?cursor->target:actual,delta,steps);
}
static inline int volume_percent(float level)
{
    if(!(level >= 0.0f)) return -1;
    if(level > 1.0f) level = 1.0f;
    return (int)(level * 100.0f + 0.5f);
}

#endif
