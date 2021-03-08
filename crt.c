#include "pico/stdlib.h"
#include <stdlib.h>
#include <string.h>

#include "hardware/dma.h"
#include "hardware/clocks.h"

#include "test.pio.h"
#include "crt.h"

video_buffers *createVideoBuffers() {
  video_buffers *buffers = malloc(sizeof(video_buffers));
  memset(&buffers->buffer1, 0, VIDEO_BUFFER_SIZE);
  memset(&buffers->buffer2, 0, VIDEO_BUFFER_SIZE);
  buffers->backBuffer = &buffers->buffer2;
  buffers->frontBuffer = &buffers->buffer1;
  buffers->bufferSelectDMAChannel = 0;
  buffers->videoDMAChannel = 0;
  return buffers;
}

void initVideoPIO(PIO pio, uint video_pin, uint hsync_pin, uint vsync_pin) {
  float clock_freq = clock_get_hz(clk_sys);
  float dot_freq = 15667200;

  uint hsync_offset = pio_add_program(pio, &hsync_program);
  pio_sm_config hsync_config = hsync_program_get_default_config(hsync_offset);
  pio_gpio_init(pio, hsync_pin);
  pio_sm_set_consecutive_pindirs(pio, HSYNC_SM, hsync_pin, 1, true);
  sm_config_set_sideset_pins(&hsync_config, hsync_pin);
  sm_config_set_clkdiv(&hsync_config, clock_freq / dot_freq);

  uint vsync_offset = pio_add_program(pio, &vsync_program);
  pio_sm_config vsync_config = vsync_program_get_default_config(vsync_offset);
  pio_gpio_init(pio, vsync_pin);
  pio_sm_set_consecutive_pindirs(pio, VSYNC_SM, vsync_pin, 1, true);
  sm_config_set_sideset_pins(&vsync_config, vsync_pin);
  // Autopull initial config
  sm_config_set_out_shift(&vsync_config, false, true, 32);
  sm_config_set_clkdiv(&vsync_config, clock_freq / (2 * dot_freq));

  uint video_offset = pio_add_program(pio, &video_program);
  pio_sm_config video_config = video_program_get_default_config(video_offset);
  pio_gpio_init(pio, video_pin);
  sm_config_set_out_pins(&video_config, video_pin, 1);
  sm_config_set_sideset_pins(&video_config, video_pin);
  // Autopull
  sm_config_set_out_shift(&video_config, false, true, 32);
  // Use ISR for arithmetic
  sm_config_set_in_shift(&video_config, false, false, 0);
  pio_sm_set_consecutive_pindirs(pio, VIDEO_SM, video_pin, 1, true);
  sm_config_set_clkdiv(&video_config, clock_freq / (2 * dot_freq));

  pio_sm_init(pio, HSYNC_SM, hsync_offset, &hsync_config);
  pio_sm_init(pio, VSYNC_SM, vsync_offset, &vsync_config);
  pio_sm_init(pio, VIDEO_SM, video_offset, &video_config);

  pio->txf[VSYNC_SM] = 341;
  pio->txf[VIDEO_SM] = 511;
}

void initVideoDMA(video_buffers *buffers) {
  int buffer_select_chan = dma_claim_unused_channel(true);
  int video_chan = dma_claim_unused_channel(true);

  // Copy the address of the current front buffer to the video channel trigger
  dma_channel_config buffer_select_config = dma_channel_get_default_config(buffer_select_chan);
  channel_config_set_transfer_data_size(&buffer_select_config, DMA_SIZE_32);
  // Always copy the front buffer pointer
  channel_config_set_read_increment(&buffer_select_config, false);
  // Always copy to video channel trigger
  channel_config_set_write_increment(&buffer_select_config, false);

  dma_channel_configure(
    buffer_select_chan,
    &buffer_select_config,
    &dma_hw->ch[video_chan].al3_read_addr_trig, // Write to video read address trigger
    &buffers->frontBuffer, // Read the pointer to front buffer
    1, // Copy pointer and stop
    false // Don't start yet
  );

  // Then copy the frame of video data from the buffer to the crt pio state machine
  dma_channel_config video_config = dma_channel_get_default_config(video_chan);
  channel_config_set_transfer_data_size(&video_config, DMA_SIZE_32);
  channel_config_set_read_increment(&video_config, true);
  channel_config_set_dreq(&video_config, DREQ_PIO0_TX0);
  // And loop back to sending resolution
  channel_config_set_chain_to(&video_config, buffer_select_chan);

  dma_channel_configure(
      video_chan,
      &video_config,
      &pio0_hw->txf[0], // Write address (only need to set this once)
      NULL,             // Don't provide a read address yet
      VIDEO_BUFFER_LENGTH, // Write the same value many times, then halt and interrupt
      false             // Don't start yet
  );

  buffers->bufferSelectDMAChannel = buffer_select_chan;
  buffers->videoDMAChannel = video_chan;
}

void startVideo(video_buffers *buffers, PIO pio) {
  dma_channel_start(buffers->bufferSelectDMAChannel);
  // Pre-fill PIO TX queue
  while (
    dma_hw->ch[buffers->videoDMAChannel].al3_transfer_count == VIDEO_BUFFER_LENGTH
    ) {
    tight_loop_contents();
  }
  pio_enable_sm_mask_in_sync(pio, 0b111);
}

void swapBuffers(video_buffers *buffers) {
  uint32_t *tempBuffer = buffers->frontBuffer;
  buffers->frontBuffer = buffers->backBuffer;
  buffers->backBuffer = tempBuffer;
}

