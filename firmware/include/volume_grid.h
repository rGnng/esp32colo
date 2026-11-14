#ifndef MPDTAB5_VOLUME_GRID_H
#define MPDTAB5_VOLUME_GRID_H

/* Match Headunit: round to the nearest grid position, then move whole notches.
 * MPD accepts integer percentages, so round each position to 0..100 as well. */
static inline int volume_steps_normalize(int steps) { return steps == 64 ? 64 : 16; }
static inline int volume_grid_step(int level, int delta, int steps)
{
    steps = volume_steps_normalize(steps);
    if(level < 0) level = 0;
    if(level > 100) level = 100;
    int index = (level * steps + 50) / 100;
    if(delta > steps - index) index = steps;
    else if(delta < -index) index = 0;
    else index += delta;
    return (index * 100 + steps / 2) / steps;
}
static inline int volume_grid_snap(int level, int steps)
{
    return volume_grid_step(level, 0, steps);
}

#endif
