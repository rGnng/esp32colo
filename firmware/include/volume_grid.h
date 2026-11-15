#ifndef MPDTAB5_VOLUME_GRID_H
#define MPDTAB5_VOLUME_GRID_H

/* Normalized 0..1 levels retain exact binary 1/16 and 1/64 positions.
 * Convert to integer percentages only at the MPD/legacy display boundary. */
static inline int volume_steps_normalize(int steps) { return steps == 64 ? 64 : 16; }
static inline float volume_grid_step(float level, int delta, int steps)
{
    steps = volume_steps_normalize(steps);
    if(!(level >= 0.0f)) level = 0.0f;
    if(level > 1.0f) level = 1.0f;
    int index = (int)(level * steps + 0.5f);
    if(delta > steps - index) index = steps;
    else if(delta < -index) index = 0;
    else index += delta;
    return (float)index / steps;
}
static inline float volume_grid_snap(float level, int steps)
{
    return volume_grid_step(level, 0, steps);
}
static inline int volume_percent(float level)
{
    if(!(level >= 0.0f)) return -1;
    if(level > 1.0f) level = 1.0f;
    return (int)(level * 100.0f + 0.5f);
}

#endif
