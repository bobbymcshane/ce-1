#include "ce_vim.h"

#include <string.h>
#include <ctype.h>
#include <ncurses.h>

// TODO: move these to utils somewhere?
static int64_t istrtol(const CeRune_t* istr, const CeRune_t** end_of_numbers){
     int64_t value = 0;
     const CeRune_t* itr = istr;

     while(*itr){
          if(isdigit(*itr)){
               value *= 10;
               value += *itr - '0';
          }else{
               if(itr != istr) *end_of_numbers = itr;
               break;
          }

          itr++;
     }

     return value;
}

static int64_t istrlen(const CeRune_t* istr){
     int64_t len = 0;
     while(*istr) istr++;
     return len;
}

bool ce_vim_init(CeVim_t* vim){
     vim->chain_undo = false;

     vim->key_binds[0].key = 'i';
     vim->key_binds[0].function = &ce_vim_parse_set_insert_mode;
     vim->key_bind_count = 1;

     return true;
}

bool ce_vim_free(CeVim_t* vim){
     (void)(vim);
     return true;
}

bool ce_vim_bind_key(CeVim_t* vim, CeRune_t key, CeVimParseFunc_t function){
     for(int64_t i = 0; i < vim->key_bind_count; ++i){
          if(vim->key_binds[i].key == key){
               vim->key_binds[i].function = function;
               return true;
          }
     }

     if(vim->key_bind_count >= CE_VIM_MAX_KEY_BINDS) return false;

     vim->key_binds[vim->key_bind_count].key = key;
     vim->key_binds[vim->key_bind_count].function = function;
     vim->key_bind_count++;
     return true;
}

CeVimParseResult_t ce_vim_handle_key(CeVim_t* vim, CeView_t* view, CeRune_t key, CeConfigOptions_t* config_options){
     switch(vim->mode){
     default:
          return CE_VIM_PARSE_INVALID;
     case CE_VIM_MODE_INSERT:
          switch(key){
          default:
               if(isprint(key) || key == CE_NEWLINE){
                    if(ce_buffer_insert_rune(view->buffer, key, view->cursor)){
                         const char str[2] = {key, 0};
                         CePoint_t new_cursor = ce_buffer_advance_point(view->buffer, view->cursor, 1);

                         // TODO: convenience function
                         CeBufferChange_t change = {};
                         change.chain = vim->chain_undo;
                         change.insertion = true;
                         change.remove_line_if_empty = true;
                         change.string = strdup(str);
                         change.location = view->cursor;
                         change.cursor_before = view->cursor;
                         change.cursor_after = new_cursor;
                         ce_buffer_change(view->buffer, &change);

                         view->cursor = new_cursor;
                         vim->chain_undo = true;
                    }
               }
               break;
          case KEY_BACKSPACE:
               if(!ce_points_equal(view->cursor, (CePoint_t){0, 0})){
                    CePoint_t remove_point = ce_buffer_advance_point(view->buffer, view->cursor, -1);
                    char* removed_string = ce_buffer_dupe_string(view->buffer, remove_point, 1);
                    if(ce_buffer_remove_string(view->buffer, remove_point, 1, true)){

                         // TODO: convenience function
                         CeBufferChange_t change = {};
                         change.chain = vim->chain_undo;
                         change.insertion = false;
                         change.remove_line_if_empty = true;
                         change.string = removed_string;
                         change.location = remove_point;
                         change.cursor_before = view->cursor;
                         change.cursor_after = remove_point;
                         ce_buffer_change(view->buffer, &change);

                         view->cursor = remove_point;
                         vim->chain_undo = true;
                    }
               }
               break;
          case KEY_LEFT:
               view->cursor = ce_buffer_move_point(view->buffer, view->cursor, (CePoint_t){-1, 0}, config_options->tab_width, true);
               view->scroll = ce_view_follow_cursor(view, config_options->horizontal_scroll_off, config_options->vertical_scroll_off, config_options->tab_width);
               vim->chain_undo = false;
               break;
          case KEY_DOWN:
               view->cursor = ce_buffer_move_point(view->buffer, view->cursor, (CePoint_t){0, 1}, config_options->tab_width, true);
               view->scroll = ce_view_follow_cursor(view, config_options->horizontal_scroll_off, config_options->vertical_scroll_off, config_options->tab_width);
               vim->chain_undo = false;
               break;
          case KEY_UP:
               view->cursor = ce_buffer_move_point(view->buffer, view->cursor, (CePoint_t){0, -1}, config_options->tab_width, true);
               view->scroll = ce_view_follow_cursor(view, config_options->horizontal_scroll_off, config_options->vertical_scroll_off, config_options->tab_width);
               vim->chain_undo = false;
               break;
          case KEY_RIGHT:
               view->cursor = ce_buffer_move_point(view->buffer, view->cursor, (CePoint_t){1, 0}, config_options->tab_width, true);
               view->scroll = ce_view_follow_cursor(view, config_options->horizontal_scroll_off, config_options->vertical_scroll_off, config_options->tab_width);
               vim->chain_undo = false;
               break;
          case 27: // escape
               vim->mode = CE_VIM_MODE_NORMAL;
               break;
          }
          break;
     case CE_VIM_MODE_NORMAL:
     {
          CeVimAction_t action = {};

          // append key to command
          int64_t command_len = istrlen(vim->current_command);
          if(command_len < (CE_VIM_MAX_COMMAND_LEN - 1)){
               vim->current_command[command_len] = key;
               vim->current_command[command_len + 1] = 0;
          }

          CeVimParseResult_t result = ce_vim_parse_action(&action, vim->current_command, vim->key_binds, vim->key_bind_count);

          if(result == CE_VIM_PARSE_COMPLETE){
               ce_vim_apply_action(&action, view->buffer, &view->cursor, vim);
               vim->current_command[0] = 0;
          }

          return result;
     } break;
     }

     return CE_VIM_PARSE_COMPLETE;
}

CeVimParseResult_t ce_vim_parse_action(CeVimAction_t* action, const CeRune_t* keys, CeVimKeyBind_t* key_binds,
                                       int64_t key_bind_count){
     CeVimParseResult_t result = CE_VIM_PARSE_INVALID;
     CeVimAction_t build_action = {};

     // parse multiplier if it exists
     const CeRune_t* end_of_multiplier = NULL;
     int64_t multiplier = istrtol(keys, &end_of_multiplier);
     if(end_of_multiplier){
          build_action.motion.multiplier = multiplier;
          keys = end_of_multiplier;
     }

     // parse verb
     for(int64_t i = 0; i < key_bind_count; ++i){
          CeVimKeyBind_t* key_bind = key_binds + i;
          if(*keys == key_bind->key){
               result = key_bind->function(&build_action);
               if(result != CE_VIM_PARSE_KEY_NOT_HANDLED){
                    keys++;
                    break;
               }
          }
     }

     // parse multiplier
     if(result != CE_VIM_PARSE_COMPLETE){
          multiplier = istrtol(keys, &end_of_multiplier);
          if(end_of_multiplier){
               build_action.motion.multiplier = multiplier;
               keys = end_of_multiplier;
          }

          // parse motion
          for(int64_t i = 0; i < key_bind_count; ++i){
               CeVimKeyBind_t* key_bind = key_binds + i;
               if(*keys == key_bind->key){
                    result = key_bind->function(&build_action);
                    if(result != CE_VIM_PARSE_KEY_NOT_HANDLED) break;
               }
          }
     }

     if(result == CE_VIM_PARSE_COMPLETE) *action = build_action;
     return result;
}

bool ce_vim_apply_action(const CeVimAction_t* action, CeBuffer_t* buffer, CePoint_t* cursor, CeVim_t* vim){
     CeMotionRange_t motion_range = {*cursor, *cursor, NULL, NULL};
     bool success = true;
     if(action->motion.function) motion_range = action->motion.function(action, vim, buffer, cursor);
     if(ce_point_after(motion_range.a, motion_range.b)){
          motion_range.first = &motion_range.b;
          motion_range.last = &motion_range.a;
     }else{
          motion_range.first = &motion_range.a;
          motion_range.last = &motion_range.b;
     }
     if(action->verb.function) success = action->verb.function(action, &motion_range, vim, buffer);
     vim->mode = action->end_in_mode;
     return success;
}

CeVimParseResult_t ce_vim_parse_set_insert_mode(CeVimAction_t* action){
     if(action->motion.function) return CE_VIM_PARSE_KEY_NOT_HANDLED;
     if(action->verb.function) return CE_VIM_PARSE_KEY_NOT_HANDLED;

     action->end_in_mode = CE_VIM_MODE_INSERT;
     return CE_VIM_PARSE_COMPLETE;
}
