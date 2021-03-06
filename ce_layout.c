#include "ce_layout.h"
#include "ce_app.h"

#include <stdlib.h>
#include <assert.h>

CeLayout_t* ce_layout_tab_list_init(CeLayout_t* tab_layout){
     CeLayout_t* tab_list_layout = calloc(1, sizeof(*tab_list_layout));
     if(!tab_list_layout) return NULL;
     tab_list_layout->type = CE_LAYOUT_TYPE_TAB_LIST;
     tab_list_layout->tab_list.tabs = malloc(sizeof(*tab_list_layout->tab_list.tabs));
     tab_list_layout->tab_list.tabs[0] = tab_layout;
     tab_list_layout->tab_list.tab_count = 1;
     tab_list_layout->tab_list.current = tab_layout;
     return tab_list_layout;
}

CeLayout_t* ce_layout_tab_list_add(CeLayout_t* tab_list_layout){
     assert(tab_list_layout->type == CE_LAYOUT_TYPE_TAB_LIST);
     int64_t new_tab_count = tab_list_layout->tab_list.tab_count + 1;
     CeLayout_t** new_tabs = realloc(tab_list_layout->tab_list.tabs, new_tab_count * sizeof(*tab_list_layout->tab_list.tabs));
     if(!new_tabs) return false;
     tab_list_layout->tab_list.tabs = new_tabs;
     new_tabs[tab_list_layout->tab_list.tab_count] = ce_layout_tab_init(tab_list_layout->tab_list.current->tab.current->view.buffer);
     tab_list_layout->tab_list.tab_count = new_tab_count;
     return new_tabs[tab_list_layout->tab_list.tab_count - 1];
}

CeLayout_t* ce_layout_view_init(CeBuffer_t* buffer){
     CeLayout_t* view_layout = calloc(1, sizeof(*view_layout));
     if(!view_layout) return NULL;
     view_layout->type = CE_LAYOUT_TYPE_VIEW;
     view_layout->view.buffer = buffer;
     view_layout->view.user_data = calloc(1, sizeof(CeAppViewData_t));
     return view_layout;
}

CeLayout_t* ce_layout_tab_init(CeBuffer_t* buffer){
     CeLayout_t* view_layout = ce_layout_view_init(buffer);

     CeLayout_t* tab_layout = calloc(1, sizeof(*tab_layout));
     if(!tab_layout) return NULL; // LEAK: leak view_layout
     tab_layout->type = CE_LAYOUT_TYPE_TAB;
     tab_layout->tab.root = view_layout;
     tab_layout->tab.current = view_layout;
     return tab_layout;
}

void ce_layout_free(CeLayout_t** root){
     CeLayout_t* layout = *root;
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          free(layout->view.user_data);
          layout->view.user_data = NULL;
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               ce_layout_free(&layout->list.layouts[i]);
          }
          free(layout->list.layouts);
          layout->list.layout_count = 0;
          break;
     case CE_LAYOUT_TYPE_TAB:
          ce_layout_free(&layout->tab.root);
          break;
     case CE_LAYOUT_TYPE_TAB_LIST:
          for(int64_t i = 0; i < layout->tab_list.tab_count; i++){
               ce_layout_free(&layout->tab_list.tabs[i]);
          }
          free(layout->tab_list.tabs);
          layout->tab_list.tab_count = 0;
          break;
     }

     free(*root);
     *root = NULL;
}

static CeBuffer_t* ce_layout_find_buffer(CeLayout_t* layout){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          if(layout->view.buffer) return layout->view.buffer;
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               CeBuffer_t* found = ce_layout_find_buffer(layout->list.layouts[i]);
               if(found) return found;
          }
          break;
     case CE_LAYOUT_TYPE_TAB:
          return ce_layout_find_buffer(layout->tab.root);
     case CE_LAYOUT_TYPE_TAB_LIST:
          for(int64_t i = 0; i < layout->tab_list.tab_count; i++){
               CeBuffer_t* buffer = ce_layout_find_buffer(layout->tab_list.tabs[i]);
               if(buffer) return buffer;
          }
          break;
     }

     return NULL;
}

CeLayout_t* ce_layout_split(CeLayout_t* layout, bool vertical){
     assert(layout->type == CE_LAYOUT_TYPE_TAB);
     CeLayout_t* parent_of_current = ce_layout_find_parent(layout, layout->tab.current);
     if(parent_of_current){
          CeBuffer_t* buffer = ce_layout_find_buffer(layout->tab.current);
          assert(buffer);
          switch(parent_of_current->type){
          default:
               break;
          case CE_LAYOUT_TYPE_LIST:
               if(parent_of_current->list.vertical == vertical){
                    CeLayout_t* new_layout = ce_layout_view_init(buffer);
                    if(!new_layout) return NULL;
                    new_layout->view.scroll = layout->tab.current->view.scroll;
                    new_layout->view.cursor = layout->tab.current->view.cursor;

                    int64_t new_layout_count = parent_of_current->list.layout_count + 1;
                    parent_of_current->list.layouts = realloc(parent_of_current->list.layouts,
                                                              new_layout_count * sizeof(*parent_of_current->list.layouts));
                    if(parent_of_current->list.layouts) parent_of_current->list.layout_count = new_layout_count;
                    else return NULL;
                    for(int64_t i = parent_of_current->list.layout_count - 1; i > 0; i--){
                         parent_of_current->list.layouts[i] = parent_of_current->list.layouts[i - 1];
                    }
                    parent_of_current->list.layouts[0] = new_layout;
                    return new_layout;
               }else{
                    CeLayout_t* new_list_layout = calloc(1, sizeof(*new_list_layout));
                    if(!new_list_layout) return NULL;
                    new_list_layout->type = CE_LAYOUT_TYPE_LIST;
                    new_list_layout->list.layouts = malloc(sizeof(*new_list_layout->list.layouts));
                    if(!new_list_layout->list.layouts) return NULL;
                    new_list_layout->list.layout_count = 1;
                    new_list_layout->list.vertical = vertical;
                    new_list_layout->list.layouts[0] = layout->tab.current;

                    // point parent at new list
                    for(int64_t i = 0; i < parent_of_current->list.layout_count; i++){
                         if(parent_of_current->list.layouts[i] == layout->tab.current){
                              parent_of_current->list.layouts[i] = new_list_layout;
                              break;
                         }
                    }

                    return ce_layout_split(layout, vertical);
               }
               break;
          case CE_LAYOUT_TYPE_TAB:
          {
               CeLayout_t* list_layout = calloc(1, sizeof(*list_layout));
               list_layout->type = CE_LAYOUT_TYPE_LIST;
               list_layout->list.layout_count = 1;
               list_layout->list.layouts = calloc(list_layout->list.layout_count, sizeof(*list_layout->list.layouts));
               list_layout->list.layouts[0] = layout->tab.current;
               list_layout->list.vertical = vertical;

               parent_of_current->tab.root = list_layout;

               return ce_layout_split(layout, vertical);
          } break;
          case CE_LAYOUT_TYPE_TAB_LIST:
               if(layout->tab_list.current) return ce_layout_split(layout->tab_list.current, vertical);
               break;
          }
     }

     return NULL;
}

void ce_layout_distribute_rect(CeLayout_t* layout, CeRect_t rect){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          layout->view.rect = rect;
          break;
     case CE_LAYOUT_TYPE_LIST:
     {
          layout->list.rect = rect;
          CeRect_t sliced_rect = rect;
          int64_t separator_lines = layout->list.layout_count - 1;

          if(layout->list.vertical){
               int64_t rect_height = rect.bottom - rect.top;
               int64_t slice_height = (rect_height - separator_lines) / layout->list.layout_count;
               int64_t leftover_lines = (rect_height - separator_lines) % layout->list.layout_count;
               sliced_rect.bottom = sliced_rect.top + slice_height;

               for(int64_t i = 0; i < layout->list.layout_count; i++){
                    if(leftover_lines > 0){
                         leftover_lines--;
                         sliced_rect.bottom++;
                    }

                    ce_layout_distribute_rect(layout->list.layouts[i], sliced_rect);

                    sliced_rect.top = sliced_rect.bottom + 1;
                    sliced_rect.bottom = sliced_rect.top + slice_height;
               }
          }else{
               int64_t rect_width = rect.right - rect.left;
               int64_t slice_width = (rect_width - separator_lines) / layout->list.layout_count;
               int64_t leftover_lines = (rect_width - separator_lines) % layout->list.layout_count;
               sliced_rect.right = sliced_rect.left + slice_width;

               for(int64_t i = 0; i < layout->list.layout_count; i++){
                    if(leftover_lines > 0){
                         leftover_lines--;
                         sliced_rect.right++;
                    }

                    ce_layout_distribute_rect(layout->list.layouts[i], sliced_rect);

                    sliced_rect.left = sliced_rect.right + 1;
                    sliced_rect.right = sliced_rect.left + slice_width;
               }
          }
     } break;
     case CE_LAYOUT_TYPE_TAB:
          layout->tab.rect = rect;
          ce_layout_distribute_rect(layout->tab.root, rect);
          break;
     case CE_LAYOUT_TYPE_TAB_LIST:
          layout->tab_list.rect = rect;
          if(layout->tab_list.tab_count > 1) rect.top++; // leave space for a tab bar, this probably doesn't want to be built in?
          ce_layout_distribute_rect(layout->tab_list.current, rect);
          break;
     }
}

CeLayout_t* ce_layout_find_at(CeLayout_t* layout, CePoint_t point){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          if(ce_point_in_rect(point, layout->view.rect)) return layout;
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               CeLayout_t* found = ce_layout_find_at(layout->list.layouts[i], point);
               if(found) return found;
          }
          break;
     case CE_LAYOUT_TYPE_TAB:
          return ce_layout_find_at(layout->tab.root, point);
     case CE_LAYOUT_TYPE_TAB_LIST:
          return ce_layout_find_at(layout->tab_list.current, point);
     }

     return NULL;
}

CeLayout_t* ce_layout_find_parent(CeLayout_t* root, CeLayout_t* node){
     switch(root->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < root->list.layout_count; i++){
               if(root->list.layouts[i] == node) return root;
               CeLayout_t* found = ce_layout_find_parent(root->list.layouts[i], node);
               if(found) return found;
          }
          break;
     case CE_LAYOUT_TYPE_TAB:
          if(root->tab.root == node) return root;
          else return ce_layout_find_parent(root->tab.root, node);
          break;
     case CE_LAYOUT_TYPE_TAB_LIST:
          for(int64_t i = 0; i < root->tab_list.tab_count; i++){
               if(root->tab_list.tabs[i] == node) return root;
               CeLayout_t* found = ce_layout_find_parent(root->tab_list.tabs[i], node);
               if(found) return found;
          }
          break;
     }

     return NULL;
}

bool ce_layout_delete(CeLayout_t* root, CeLayout_t* node){
     CeLayout_t* parent = ce_layout_find_parent(root, node);
     if(!parent) return false;

     switch(parent->type){
     default:
          return false;
     case CE_LAYOUT_TYPE_LIST:
     {
          // find index of matching node
          int64_t index = -1;
          for(int64_t i = 0; i < parent->list.layout_count; i++){
               if(parent->list.layouts[i] == node){
                    index = i;
                    break;
               }
          }

          if(index == -1) return false;

          ce_layout_free(&node);

          // remove element, keeping element order
          int64_t new_count = parent->list.layout_count - 1;
          for(int64_t i = index; i < new_count; i++){
               parent->list.layouts[i] = parent->list.layouts[i + 1];
          }
          parent->list.layout_count = new_count;

          if(new_count == 0) return ce_layout_delete(root, parent);
     } break;
     case CE_LAYOUT_TYPE_TAB:
          break;
     case CE_LAYOUT_TYPE_TAB_LIST:
     {
          // find index of matching node
          int64_t index = -1;
          for(int64_t i = 0; i < parent->tab_list.tab_count; i++){
               if(parent->tab_list.tabs[i] == node){
                    index = i;
                    break;
               }
          }

          if(index == -1) return false;

          ce_layout_free(&node);

          // remove element, keeping element order
          int64_t new_count = parent->tab_list.tab_count - 1;
          for(int64_t i = index; i < new_count; i++){
               parent->tab_list.tabs[i] = parent->tab_list.tabs[i + 1];
          }
          parent->tab_list.tab_count = new_count;
     } break;
     }

     return true;
}

CeLayout_t* ce_layout_buffer_in_view(CeLayout_t* layout, CeBuffer_t* buffer){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          if(layout->view.buffer == buffer) return layout;
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               CeLayout_t* found = ce_layout_buffer_in_view(layout->list.layouts[i], buffer);
               if(found) return found;
          }
          break;
     case CE_LAYOUT_TYPE_TAB:
          return ce_layout_buffer_in_view(layout->tab.root, buffer);
     case CE_LAYOUT_TYPE_TAB_LIST:
          return ce_layout_buffer_in_view(layout->tab_list.current, buffer);
     }

     return NULL;
}

int64_t count_buffer_in_views(CeLayout_t* layout, CeBuffer_t* buffer, int64_t count){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          if(layout->view.buffer == buffer) return 1;
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               count = count_buffer_in_views(layout->list.layouts[i], buffer, count);
          }
          break;
     case CE_LAYOUT_TYPE_TAB:
          return count_buffer_in_views(layout->tab.root, buffer, count);
     case CE_LAYOUT_TYPE_TAB_LIST:
          return count_buffer_in_views(layout->tab_list.current, buffer, count);
     }

     return count;
}

void build_buffer_in_views(CeLayout_t* layout, CeBuffer_t* buffer, CeLayoutBufferInViewsResult_t* result){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          if(layout->view.buffer == buffer){
               result->layouts[result->layout_count] = layout;
               result->layout_count++;
          }
          break;
     case CE_LAYOUT_TYPE_LIST:
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               build_buffer_in_views(layout->list.layouts[i], buffer, result);
          }
          break;
     case CE_LAYOUT_TYPE_TAB:
          build_buffer_in_views(layout->tab.root, buffer, result);
          break;
     case CE_LAYOUT_TYPE_TAB_LIST:
          build_buffer_in_views(layout->tab_list.current, buffer, result);
          break;
     }
}

CeLayoutBufferInViewsResult_t ce_layout_buffer_in_views(CeLayout_t* layout, CeBuffer_t* buffer){
     CeLayoutBufferInViewsResult_t result = {};
     int64_t count = count_buffer_in_views(layout, buffer, 0);
     result.layouts = malloc(count * sizeof(*result.layouts));
     build_buffer_in_views(layout, buffer, &result);
     return result;
}

static int64_t count_children(CeLayout_t* layout){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_VIEW:
          return 1;
     case CE_LAYOUT_TYPE_LIST:
     {
          int sum = 0;
          for(int64_t i = 0; i < layout->list.layout_count; i++){
               sum += count_children(layout->list.layouts[i]);
          }
          return sum;
     }
     }

     return 0;
}

int64_t ce_layout_tab_get_layout_count(CeLayout_t* layout){
     switch(layout->type){
     default:
          break;
     case CE_LAYOUT_TYPE_TAB:
          if(!layout->tab.root) return 0;
          return count_children(layout->tab.root);
     }

     return 0;
}
