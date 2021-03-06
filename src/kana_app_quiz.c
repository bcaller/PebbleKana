#include <pebble.h>
#include <pebble_fonts.h>
#include "kana_app_quiz.h"
#include "kana_app_resources.h"
#include "kana_app_glyphs.h"
#include "kana_app_settings.h"
#include "colors.h"

#ifdef PBL_COLOR
  #define FG_COLOR COLOR_TEXT
  #define SEC_FG_COLOR COLOR_TEXT
#endif
#define GLYPH_DIMENSIONS 120

//simple min! use with causion!
#define MIN(__a__, __b__) ((__a__) < (__b__) ? (__a__) : (__b__))

#ifdef PBL_COLOR
  GColor kana_app_bitmap_pallete[2];
  GColor kana_app_bitmap_error_pallete[2];
#endif

// Private variables
static int choosen_question_mode;
static int previous_question;

static int total_question_num;
static int correct_anserted_questions;
static bool already_failed_question = false;
static char stats_correct_buff[] = "0000";
static char stats_total_buff[] = "0000";
static char stats_buff[] = "0000/0000";

// Structs definitions
static struct Question {
  int correct_answer_num;
  int answer[3];
  bool enabled[3];

} currentQuestion;

static struct Ui {
  Window *window;

  int glyph_paths_num;
  int question_mode;
  GPath *glyph_path[ MAX_GLYPH_SEGMENTS ];
  Layer *glyph_layer;

  TextLayer* stats;

  TextLayer* romajiQuestion;
  TextLayer* romajiAnswer[3];    

  GBitmap *bitmaps[3];
  BitmapLayer* bitmapLayers[3]; 
} ui;

// Updaters

#ifdef PBL_COLOR
  static void bg_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    GRect rect_bottom = bounds;

    graphics_context_set_fill_color(ctx, COLOR_BG);
    graphics_fill_rect(ctx, rect_bottom, 0, GCornerNone);
  }
#endif

static void glyph_layer_update_proc(Layer *layer, GContext *ctx) {    
  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2 - 50, 0);

  for(int i=0; i < ui.glyph_paths_num; i++) {
    gpath_move_to(ui.glyph_path[i], center);

    #ifdef PBL_COLOR
      graphics_context_set_fill_color(ctx, FG_COLOR);
      graphics_context_set_stroke_color(ctx, FG_COLOR);
    #endif

    gpath_draw_filled(ctx, ui.glyph_path[i]); 
  }
}

// Private functions

static void clean_bitmap_answer(int i) {
  if(ui.bitmaps[i] != NULL) {
    bitmap_layer_destroy(ui.bitmapLayers[i]);  
    gbitmap_destroy(ui.bitmaps[i]);
    ui.bitmaps[i] = NULL;
  }
}

static void clean() {
  already_failed_question = false;

  for(int i=0; i < ui.glyph_paths_num; i++) 
    gpath_destroy(ui.glyph_path[i]);
  ui.glyph_paths_num = 0;

  for(int i=0; i < 3; i++) {
    text_layer_set_text(ui.romajiAnswer[i], "");
    #ifdef PBL_COLOR
      text_layer_set_text_color(ui.romajiAnswer[i], SEC_FG_COLOR);
    #endif
  }
  
  if(ui.romajiQuestion != NULL) {
    text_layer_destroy(ui.romajiQuestion);
    ui.romajiQuestion = NULL;
  }
  
  for(int i=0; i < 3; i++)
    clean_bitmap_answer(i);
}

static void load_bitmap_answer(int position, bool useErrorPallete) {
  #ifndef PBL_COLOR
    if(useErrorPallete) return;
  #endif

  GRect answerBounds = GRect(144 - 34, 84 - 58 + position * 49, 30, 18);

  if(choosen_question_mode == QUESTION_MODE_KATAKANA) {        
    ui.bitmaps[position] = 
      gbitmap_create_with_resource(kana_app_getKatakana(currentQuestion.answer[position]));
  } else if(choosen_question_mode == QUESTION_MODE_HIRAGANA) {
    ui.bitmaps[position] = 
      gbitmap_create_with_resource(kana_app_getHiragana(currentQuestion.answer[position]));                  
  }

  #ifdef PBL_COLOR
    gbitmap_set_palette (
      ui.bitmaps[position], 
      useErrorPallete ?
         kana_app_bitmap_error_pallete : kana_app_bitmap_pallete, 
      false);
  #endif

  ui.bitmapLayers[position] = bitmap_layer_create(answerBounds);

  bitmap_layer_set_bitmap (
    ui.bitmapLayers[position], 
    ui.bitmaps[position] );

  layer_add_child (
    window_get_root_layer(ui.window), 
    bitmap_layer_get_layer(ui.bitmapLayers[position]) );    
}

static void load_new_question() {  
  clean();

  int displayRomaji = kana_app_settings_get_setting(SETTING_ROMAJI_MODE);

  currentQuestion.correct_answer_num = rand() % 3;
  choosen_question_mode = kana_app_settings_get_setting(SETTING_QUIZ_MODE);
  choosen_question_mode = 
    (choosen_question_mode == QUESTION_MODE_RANDOM) ? 
      rand() % QUESTION_MODE_RANDOM : choosen_question_mode;
  
  for(int i=0; i < 3; i++) {
    currentQuestion.enabled[i] = true;

    bool duplicate = false;
    do {
      duplicate = false;
      currentQuestion.answer[i] = rand() % GLYPHS_NUM;

      for(int j=0; j<i; j++)
        if(currentQuestion.answer[j] == currentQuestion.answer[i]) 
          duplicate = true;

      if(previous_question == currentQuestion.answer[i]) 
        duplicate = true; 

    } while (duplicate);
  }

  previous_question = currentQuestion.answer[currentQuestion.correct_answer_num];

  int act_char_id = currentQuestion.answer[currentQuestion.correct_answer_num];
  if(displayRomaji == ROMAJI_AS_QUESTION) {
    GRect bounds = GRect(5, 56, 100, 60);           
    ui.romajiQuestion = text_layer_create(bounds);  
    text_layer_set_font(ui.romajiQuestion, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));  
    text_layer_set_text(ui.romajiQuestion, "");
    text_layer_set_text_alignment(ui.romajiQuestion, GTextAlignmentCenter);
    #ifdef PBL_COLOR
      text_layer_set_background_color(ui.romajiQuestion, GColorClear);  
      text_layer_set_text_color(ui.romajiQuestion, SEC_FG_COLOR);
    #endif

    layer_add_child(
      window_get_root_layer(ui.window), 
      text_layer_get_layer(ui.romajiQuestion));
    text_layer_set_text(
      ui.romajiQuestion, 
      kana_app_getRomaji(act_char_id));

    for(int i=0; i<3; i++) 
      load_bitmap_answer(i, false);

  } else if (displayRomaji == ROMAJI_AS_ANSWERS) {

    if(choosen_question_mode == QUESTION_MODE_KATAKANA) {
      ui.glyph_paths_num = kana_app_katakana_glyphs[act_char_id].size;
      
      for(int i = 0; i < ui.glyph_paths_num; i++) 
        ui.glyph_path[i] = gpath_create(kana_app_katakana_glyphs[act_char_id].glyph_path[i]);    

      for(int i = 0; i < 3; i++) 
        text_layer_set_text(ui.romajiAnswer[i], kana_app_getRomaji(currentQuestion.answer[i]));

    } else if(choosen_question_mode == QUESTION_MODE_HIRAGANA) {
      ui.glyph_paths_num = kana_app_hiragana_glyphs[act_char_id].size;
      
      for(int i = 0; i < ui.glyph_paths_num; i++) 
        ui.glyph_path[i] = gpath_create(kana_app_hiragana_glyphs[act_char_id].glyph_path[i]);

      for(int i = 0; i < 3; i++) 
        text_layer_set_text(ui.romajiAnswer[i], kana_app_getRomaji(currentQuestion.answer[i]));
    }
  }

  layer_mark_dirty(window_get_root_layer(ui.window));
}

static void update_stats_disp() {
  total_question_num = MIN(total_question_num, 9999);
  correct_anserted_questions = MIN(correct_anserted_questions, 9999);

  snprintf(
    stats_correct_buff,
    sizeof(stats_correct_buff), 
    "%d", 
    correct_anserted_questions);

  snprintf(
    stats_total_buff, 
    sizeof(stats_total_buff), 
    "%d", 
    total_question_num);

  stats_buff[0] = '\0';
  strcat(strcat(strcat(stats_buff, stats_correct_buff), "/"), stats_total_buff);

  text_layer_set_text(ui.stats, stats_buff);  
}

//input handlers
static void button_click(ClickRecognizerRef recognizer, void *context) {
  ButtonId button = click_recognizer_get_button_id(recognizer);
  int answerNum = 0;
  if(button == BUTTON_ID_SELECT) answerNum = 1;
  if(button == BUTTON_ID_DOWN)   answerNum = 2;

  if(currentQuestion.correct_answer_num == answerNum) {
    if(!already_failed_question) 
      correct_anserted_questions++;

    total_question_num++;
    update_stats_disp();

    load_new_question();
  } else {    
    already_failed_question = true;

    if(currentQuestion.enabled[answerNum]) {      
      if(kana_app_settings_get_setting(SETTING_VIBRATIONS) == SETTING_VIBRATIONS_YES) 
        vibes_short_pulse();

      currentQuestion.enabled[answerNum] = false;

      if(kana_app_settings_get_setting(SETTING_ROMAJI_MODE) == ROMAJI_AS_QUESTION) {        
        clean_bitmap_answer(answerNum);
        load_bitmap_answer(answerNum, true);
      } else {
        #ifdef PBL_COLOR
          text_layer_set_text_color(ui.romajiAnswer[answerNum], WRONG_ANSWER_COLOR);
        #else
          text_layer_set_text(ui.romajiAnswer[answerNum], "");
        #endif
      }
    }
  }
}

static void config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_DOWN, button_click);
  window_single_click_subscribe(BUTTON_ID_UP, button_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, button_click);
}

//loading
static void load(Window* window) {    
  previous_question = -1;

  Layer *window_layer = window_get_root_layer(ui.window);
  GRect window_bounds = layer_get_bounds(window_layer);
  #ifdef PBL_COLOR
    layer_set_update_proc(window_layer, bg_update_proc);
  #endif

  // init text layers
  for(int i=0; i < 3; i++) {
    GRect answerBounds = GRect(144 - 34, 84 - 58 + i * 49, 30, 18);

    ui.romajiAnswer[i] = text_layer_create(answerBounds);    
    text_layer_set_text_alignment(ui.romajiAnswer[i], GTextAlignmentRight);
    text_layer_set_font(ui.romajiAnswer[i], fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text(ui.romajiAnswer[i], "");

    #ifdef PBL_COLOR
      text_layer_set_background_color(ui.romajiAnswer[i], GColorClear);
      text_layer_set_text_color(ui.romajiAnswer[i], SEC_FG_COLOR);
    #endif
  }

  for(int i=0; i < 3; i++) 
    ui.bitmaps[i] = NULL;

  GRect statsBounds = GRect(0, 0, 144, 18);
  ui.stats = text_layer_create(statsBounds);
  text_layer_set_text_alignment(ui.stats, GTextAlignmentCenter);
  text_layer_set_font(ui.stats, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  #ifdef PBL_COLOR
    text_layer_set_background_color(ui.stats, GColorClear);
    text_layer_set_text_color(ui.stats, SEC_FG_COLOR);
  #endif

  total_question_num = 0;
  correct_anserted_questions = 0;
  update_stats_disp();

  // init icons
  GRect icon_rect = GRect(0, 0, window_bounds.size.w, window_bounds.size.h);
  grect_align(&icon_rect, &window_bounds, GAlignCenter, false);
  ui.glyph_layer = layer_create(icon_rect);
  layer_set_update_proc(ui.glyph_layer, glyph_layer_update_proc);
  layer_set_clips(ui.glyph_layer, true);

  // init layers
  layer_add_child(window_layer, ui.glyph_layer);  
  layer_add_child(window_layer, text_layer_get_layer(ui.stats));
  for(int i=0; i<3; i++) 
    layer_add_child(window_layer, text_layer_get_layer(ui.romajiAnswer[i])); 

  window_set_click_config_provider(ui.window, (ClickConfigProvider) config_provider);

  load_new_question();
}

static void unload(Window* window) {  
  clean();

  for(int i=0; i < 3; i++) 
    text_layer_destroy(ui.romajiAnswer[i]);
  text_layer_destroy(ui.stats);
  layer_destroy(ui.glyph_layer);
}

void kana_app_quiz_init() {
  ui.window = window_create();
  ui.glyph_paths_num = 0;

  #ifdef PBL_COLOR
  kana_app_bitmap_pallete[0] = FG_COLOR;
  kana_app_bitmap_pallete[1] = COLOR_BG;

  kana_app_bitmap_error_pallete[0] = WRONG_ANSWER_COLOR;
  kana_app_bitmap_error_pallete[1] = COLOR_BG;
  #endif

  window_set_window_handlers(ui.window,
        (WindowHandlers) {
            .load = load,
            .unload = unload
        });
}

void kana_app_quiz_show() {
  window_stack_push(ui.window, true);
}

void kana_app_quiz_deinit() {
  window_destroy(ui.window);
}
