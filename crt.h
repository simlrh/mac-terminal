#ifndef CRT_HEADER

#include "hardware/pio.h"
#include "pico/stdlib.h"

// bits
#define VIDEO_FRAME_RESOLUTION 175104
// bytes
#define VIDEO_BUFFER_SIZE 21888
// 32-bit words
#define VIDEO_BUFFER_LENGTH 5472

#define VIDEO_SM 0
#define HSYNC_SM 1
#define VSYNC_SM 2

typedef struct video_buffers
{
  void *backBuffer;
  void *frontBuffer;
  uint32_t buffer1[VIDEO_BUFFER_LENGTH];
  uint32_t buffer2[VIDEO_BUFFER_LENGTH];
  int bufferSelectDMAChannel;
  int videoDMAChannel;
} video_buffers;

video_buffers *createVideoBuffers();
void initVideoPIO(PIO pio, uint video_pin, uint hsync_pin, uint vsync_pin);
void initVideoDMA(video_buffers *buffers);
void startVideo(video_buffers *buffers, PIO pio);
void swapBuffers(video_buffers *buffers);
#else
#define CRT_HEADER
#endif
