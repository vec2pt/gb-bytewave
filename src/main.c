#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Include
#include "asm/types.h"
#include "gb/hardware.h"
#include "pitch.h"

// Include assets
#include "assets/logo.h"

// -----------------------------------------------------------------------------
// Variables / Constants
// -----------------------------------------------------------------------------

#define VERSION "v0.8.2"

#define TILE_COUNT 98

// Sound
uint16_t sound_interrupt_counter, t_bytebeat;
#define SOUND_SPEED_MIN 1
#define SOUND_SPEED_MAX 24
uint8_t sound_speed = 9, frequency_index = 12, tempo;
bool play = false;

// ByteBeat Functions
#define FUNCTIONS_COUNT 16
uint8_t function_nr = 0, function_previous_nr = 1;

// Source: http://viznut.fi/demos/unix/bytebeat_formulas.txt
const char bb_n[FUNCTIONS_COUNT][17] = {"Alien dungeon",
                                        "fractal trees",
                                        "tejeez",
                                        "Sin isn't kosher",
                                        "SADLY",
                                        "216",
                                        "bear @ celephais",
                                        "skurk+raer",
                                        "Boss level #6",
                                        "* is addictive",
                                        "#3 by wiretapped",
                                        "viznut",
                                        "spikey",
                                        "FreeFull",
                                        "Ststututterter",
                                        "Neurofunk"};

uint8_t bytebeat(uint16_t t) {
  switch (function_nr) {
  case 0:
    // Alien dungeon
    return t * (t & 16384 ? 7 : 5) * (3 + (3 & t >> 14)) >> (3 & t >> 9) |
           t >> 6;
  case 1:
    // fractal trees
    return t | t % 255 | t % 257;
  case 2:
    // tejeez (modified)
    return (t * (t >> 5 | t >> 8));
  case 3:
    // Sin() isn't kosher, people! (modified)
    return 10 * (t >> 5 | t | t >> (t >> 7)) + (7 & t >> 6);
  case 4:
    // SADLY (modified)
    return 43 * (4 * t >> 7 | 4 * t >> 4);
  case 5:
    // 216
    return t * (t ^ t + (t >> 15 | 1) ^ (t - 1280 ^ t) >> 10);
  case 6:
    // bear @ celephais
    return t + (t & t ^ t >> 6) - t * (t >> 9 & (t % 16 ? 2 : 6) & t >> 9);
  case 7:
    // skurk+raer
    return ((t & 4096) ? ((t * (t ^ t % 255) | (t >> 4)) >> 1)
                       : (t >> 3) | ((t & 8192) ? t << 2 : t));
  case 8:
    // Boss level #6
    return t * ((t & 4096 ? 6 : 16) + (1 & t >> 14)) >> (3 & t >> 8) |
           t >> (t & 4096 ? 3 : 4);
  case 9:
    // this shit is addictive
    return (t >> 6 | t << 1) + (t >> 5 | t << 3 | t >> 3) | t >> 2 | t << 1;
  case 10:
    // #3 (by wiretapped)
    return t *
           (1 +
            ((t >> 10) * (43 + (2 * (t >> (15 - ((t >> 16) % 13))) % 8))) % 8) *
           (1 + (t >> 14) % 4);
  case 11:
    // viznut
    return (t >> 6 | t | t >> (t >> 16)) * 10 + ((t >> 11) & 7);
  case 12:
    // spikey
    return ((t & ((t >> 23))) + (t | (t >> 2))) & (t >> 3) |
           (t >> 5) & (t * (t >> 7));
  case 13:
    // FreeFull
    return (~t / 100 | (t * 3)) ^ (t * 3 & (t >> 5)) & t;
  case 14:
    // Ststututterter
    return t * -(t >> 8 | t | t >> 9 | t >> 13) ^ t;
  default:
    // case 15:
    // Neurofunk
    return t * ((t & 4096 ? t % 65536 < 59392 ? 7 : t >> 6 : 16) +
                (1 & t >> 14)) >>
               (3 & -t >> (t & 2048 ? 2 : 10)) |
           t >> (t & 16384 ? t & 4096 ? 4 : 3 : 2);
  }
}

// -----------------------------------------------------------------------------
// Inputs
// -----------------------------------------------------------------------------

uint8_t previous_keys = 0;
uint8_t keys = 0;
#define DEBOUNCE_DELAY 0

#if DEBOUNCE_DELAY
// TODO: Test it
// https://gbdev.gg8.se/forums/viewtopic.php?id=964

uint16_t debounce_timer = 0;
void update_keys(void) {
  uint8_t keysState = joypad();
  previous_keys = keys;
  if ((keysState != previous_keys) && (debounce_timer >= DEBOUNCE_DELAY)) {
    keys = keysState;
    debounce_timer = 0;
  }
  debounce_timer++;
}
#else
void update_keys(void) {
  previous_keys = keys;
  keys = joypad();
}
#endif

uint8_t key_pressed(uint8_t aKey) { return keys & aKey; }
uint8_t key_ticked(uint8_t aKey) {
  return (keys & aKey) && !(previous_keys & aKey);
}
uint8_t key_released(uint8_t aKey) {
  return !(keys & aKey) && (previous_keys & aKey);
}

// -----------------------------------------------------------------------------
// Sound controle
// -----------------------------------------------------------------------------

void sound_on(void) {
  NR52_REG = 0x80;
  NR51_REG = 0x44;
  NR50_REG = 0x77;
}

void sound_off(void) {
  NR52_REG = 0x00;
  NR51_REG = 0x00;
  NR50_REG = 0x00;
}

void sound_restart(void) {
  sound_interrupt_counter = 0;
  t_bytebeat = 0;
}

// -----------------------------------------------------------------------------
// Screens configuration
// -----------------------------------------------------------------------------

#define SCREENS_NR 3
// During initialization, “previous_screen” should be different than “screen”.
uint8_t screen = 0, previous_screen = 3;

// -----------------------------------------------------------------------------
// Screen "FUNC"
// -----------------------------------------------------------------------------

void s_func_update(void) {
  set_bkg_tile_xy(1, function_previous_nr + 1, 0);
  set_bkg_tile_xy(1, function_nr + 1, 0x1E);
}

void s_func_draw(void) {
  cls();
  move_bkg(0, 0);
  gotoxy(0, 0);
  printf("FUNC");
  for (uint8_t i = 0; i != FUNCTIONS_COUNT; i++) {
    gotoxy(2, i + 1);
    printf(bb_n[i]);
  }

  set_bkg_tile_xy(17, 17, 0x60);
  set_bkg_tile_xy(18, 17, 0x36);
  set_bkg_tile_xy(19, 17, 0x33);
}

void s_func_inputs(void) {
  function_previous_nr = function_nr;
  if (key_ticked(J_DOWN)) {
    if (function_nr < FUNCTIONS_COUNT - 1)
      function_nr++;
    else
      function_nr = 0;
  } else if (key_ticked(J_UP)) {
    if (function_nr > 0)
      function_nr--;
    else
      function_nr = FUNCTIONS_COUNT - 1;
  }
}

// -----------------------------------------------------------------------------
// Screen "Visualization"
// -----------------------------------------------------------------------------

uint8_t _waveram[32];
const uint8_t *scanline_offsets = _waveram;

void scanline_isr(void) { SCX_REG = scanline_offsets[LY_REG & (uint8_t)7]; }

void remove_lcd_handlers(void) {
  // TODO: I'm not sure if this is technically correct.
  STAT_REG = STATF_HBL;
  remove_LCD(scanline_isr);
  remove_LCD(nowait_int_handler);
}

void cleanup_waveram(void) {
  for (uint8_t i = 0; i != 32; i++)
    _waveram[i] = 0;
}

void s_visualization_update(void) {
  if (play)
    scanline_offsets = _waveram + ((sys_time >> 2) & 0x07u);
}

void s_visualization_draw(void) {
  // TODO: Create better visualizations!
  cls();

  if (t_bytebeat != 0) {
    for (uint8_t i = 0; i != 32; i++) {
      uint8_t _tile[8] = {_waveram[i], _waveram[i], _waveram[i], _waveram[i],
                          _waveram[i], _waveram[i], _waveram[i], _waveram[i]};
      set_bkg_1bpp_data(TILE_COUNT + logo_TILE_COUNT + 1 + i, 1, _tile);
    }
  } else {
    // Random tiles if _waveram empty.
    for (uint8_t i = 0; i != 32; i++) {
      uint8_t r = rand();
      uint8_t _tile[8] = {r, r, r, r, r, r, r, r};
      set_bkg_1bpp_data(TILE_COUNT + logo_TILE_COUNT + 1 + i, 1, _tile);
    }
  }

  for (uint8_t i = 0; i != 32; i++)
    for (uint8_t j = 0; j != 32; j++)
      set_tile_xy(i + (j / 4) * 8, j, 0x95 + i);

  STAT_REG = STATF_MODE00;
  add_LCD(scanline_isr);
  add_LCD(nowait_int_handler);
}

// -----------------------------------------------------------------------------
// Screen "SETTINGS"
// -----------------------------------------------------------------------------

#define SETTINGS_COUNT 3
uint8_t settings_nr = 0;
// https://gbdev.io/pandocs/Audio_Registers.html?highlight=NR30#ff1a--nr30-channel-3-dac-enable
bool setting_disable_dac = true;

void s_settings_update(void) {
  gotoxy(15, 1);
  printf(pitch_class[frequency_index % 12]);
  printf("%d  ", octaves[(uint8_t)(frequency_index / 12)]);
  gotoxy(16, 2);
  tempo = SOUND_SPEED_MAX + 1 - sound_speed;
  (tempo < 10) ? printf(" %d", tempo) : printf("%d", tempo);
  gotoxy(15, 3);
  setting_disable_dac ? printf("Yes") : printf(" No");

  for (uint8_t i = 0; i != SETTINGS_COUNT; i++)
    set_bkg_tile_xy(1, i + 1, 0);
  set_bkg_tile_xy(1, settings_nr + 1, 0x1E);
}

void s_settings_draw(void) {
  cls();
  move_bkg(0, 0);
  gotoxy(0, 0);
  printf("SETTINGS");
  gotoxy(2, 1);
  printf("Pitch");
  gotoxy(2, 2);
  printf("Tempo");
  gotoxy(2, 3);
  printf("Disable DAC");

  gotoxy(0, 17);
  printf(VERSION);

  set_bkg_tile_xy(17, 17, 0x26);
  set_bkg_tile_xy(18, 17, 0x36);
  set_bkg_tile_xy(19, 17, 0x62);
}

void s_settings_inputs(void) {
  if (!key_pressed(J_A)) {
    if (key_ticked(J_UP)) {
      if (settings_nr > 0)
        settings_nr--;
      else
        settings_nr = SETTINGS_COUNT - 1;
    } else if (key_ticked(J_DOWN)) {
      if (settings_nr < SETTINGS_COUNT - 1)
        settings_nr++;
      else
        settings_nr = 0;
    }
  }
  if (key_pressed(J_A)) {
    switch (settings_nr) {
    case 0:
      // Pitch
      if (key_ticked(J_RIGHT)) {
        if (frequency_index < FREQUENCIES_COUNT - 1)
          frequency_index++;
      } else if (key_ticked(J_LEFT)) {
        if (frequency_index > 0)
          frequency_index--;
      } else if (key_ticked(J_UP)) {
        if (frequency_index < FREQUENCIES_COUNT - 12)
          frequency_index += 12;
      } else if (key_ticked(J_DOWN)) {
        if (frequency_index > 11)
          frequency_index -= 12;
      }
      break;
    case 1:
      // Tempo
      if (key_ticked(J_RIGHT)) {
        if (sound_speed > SOUND_SPEED_MIN) {
          sound_speed--;
          sound_interrupt_counter = 0;
        }
      } else if (key_ticked(J_LEFT)) {
        if (sound_speed < SOUND_SPEED_MAX) {
          sound_speed++;
          sound_interrupt_counter = 0;
        }
      }
      break;
    case 2:
      // Disable DAC
      if ((key_ticked(J_RIGHT)) || (key_ticked(J_LEFT)))
        setting_disable_dac = !setting_disable_dac;
      break;
    }
  }
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

void draw(void) {
  if (screen != previous_screen) {
    remove_lcd_handlers();
    switch (screen) {
    case 0:
      s_func_draw();
      break;
    case 1:
      s_visualization_draw();
      break;
    case 2:
      s_settings_draw();
      break;
    }
  }
  previous_screen = screen;
}

void update(void) {
  switch (screen) {
  case 0:
    s_func_update();
    break;
  case 1:
    s_visualization_update();
    break;
  case 2:
    s_settings_update();
    break;
  }
}

void check_inputs(void) {
  update_keys();

  // Navigation
  if (key_pressed(J_SELECT)) {
    if (key_ticked(J_RIGHT)) {
      if (screen < SCREENS_NR - 1)
        screen++;
      else
        screen = 0;
    } else if (key_ticked(J_LEFT)) {
      if (screen > 0)
        screen--;
      else
        screen = SCREENS_NR - 1;
    }
  }

  // Start / stop play
  else if (key_ticked(J_START)) {
    play = !play;
    if (play) {
      sound_on();
    } else {
      sound_off();
      sound_restart();
      cleanup_waveram();
    }
  }

  switch (screen) {
  case 0:
    s_func_inputs();
    break;
  case 2:
    s_settings_inputs();
    break;
  }
}

void show_logo(void) {
  gotoxy(6, 4);
  printf("Bytewave");

  // Logo
  set_bkg_tiles(5, 5, logo_WIDTH / 8, logo_HEIGHT / 8, logo_map);

  gotoxy(5, 13);
  printf("by  vec2pt");

  vsync();
  delay(2200);
}

void create_selected_char(uint8_t source_index, uint8_t target_index) {
  uint8_t tile_char[16];
  get_bkg_data(source_index, 1, tile_char);
  for (uint8_t i = 0; i < 16; i++) {
    if ((i % 2) == 0)
      tile_char[i] = 0xFF;
  }
  set_bkg_2bpp_data(target_index, 1, tile_char);
}

void setup(void) {
  font_init();
  font_load(font_ibm);

  create_selected_char(38, 96); // 'F'
  create_selected_char(54, 97); // 'V'
  create_selected_char(51, 98); // 'S'

  set_bkg_data(TILE_COUNT + 1, logo_TILE_COUNT, logo_tiles);
  show_logo();
}

void play_bytebeat(void) {
  if (setting_disable_dac)
    NR30_REG = 0x00;
  NR32_REG = 0x20;

  for (uint8_t i = 0; i < 16; i++) {
    uint8_t t_val1 = bytebeat(t_bytebeat * 32 + i);
    uint8_t t_val2 = bytebeat(t_bytebeat * 32 + i + 1);
    _waveram[i * 2] = t_val1;
    _waveram[i * 2 + 1] = t_val2;
    AUD3WAVE[i] = (t_val1 << 4) | (t_val2 & 0xFF);
  }

  NR30_REG |= 0x80;
  NR33_REG = (uint8_t)frequencies[frequency_index];
  NR34_REG = ((uint16_t)frequencies[frequency_index] >> 8) | 0x80;
}

void timer_isr(void) {
  if (play) {
    sound_interrupt_counter++;
    if (sound_interrupt_counter == sound_speed) {
      play_bytebeat();
      sound_interrupt_counter = 0;
      t_bytebeat++;
    }
  }
}

void main(void) {
  setup();
  sound_on();
  sound_restart();

  // Set up the timer interrupt.
  CRITICAL {
    // disable_interrupts();
    add_TIM(timer_isr);
    // enable_interrupts();
    TAC_REG = TACF_START | TACF_16KHZ;
    set_interrupts(IE_REG | TIM_IFLAG | LCD_IFLAG);
  }

  while (1) {
    check_inputs();
    draw();
    update();
    vsync();
  }
}
