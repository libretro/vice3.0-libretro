/*
 * soundretro.c - Simple forwarding sound device for libretro
 *
 */

#include "vice.h"

#include <stdio.h>

#include "sound.h"

extern void retro_audiocb(signed short int *sound_buffer,int sndbufsize);

static int retro_sound_init(const char *param, int *speed,
            int *fragsize, int *fragnr, int *channels)
{
  *speed = 44100;
  *fragsize = (*speed)/50/10;
  *channels = 1;

  //int buffer_size = (*fragnr) * (*channels) * (*fragsize) + 1;

  return 0;
}

static int retro_write(SWORD *pbuf, size_t nr)
{
    //printf("nr:%d ",nr);
    retro_audiocb(pbuf, nr);
    return 0;
}

static int retro_flush(char *state)
{
    //printf("flush\n");
    return 0;
}

static sound_device_t retro_device =
{
    "retro",
    retro_sound_init,
    retro_write,
    NULL,
    retro_flush,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    1
};

int sound_init_retro_device(void)
{
    return sound_register_device(&retro_device);
}
