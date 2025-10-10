#include "angband.h"
#include "sdl-config.h"
#include "log/log.h"
#include "pane.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON-based configuration system using cJSON library

static const char* pane_type_to_string(enum pane_type type)
{
    switch (type) {
        case PANE_MAIN: return "MAIN";
        case PANE_INVENTORY: return "INVENTORY";
        case PANE_WORN: return "WORN";
        case PANE_ROLLS: return "ROLLS";
        case PANE_INFO: return "INFO";
        case PANE_CHARACTER: return "CHARACTER";
        case PANE_LOG: return "LOG";
        case PANE_MONSTERS: return "MONSTERS";
        default: return "MAIN";
    }
}

static enum pane_type parse_pane_type(const char* value)
{
    if (!value)
        return PANE_MAIN;
    if (strcmp(value, "MAIN") == 0) return PANE_MAIN;
    if (strcmp(value, "INVENTORY") == 0) return PANE_INVENTORY;
    if (strcmp(value, "WORN") == 0) return PANE_WORN;
    if (strcmp(value, "ROLLS") == 0) return PANE_ROLLS;
    if (strcmp(value, "INFO") == 0) return PANE_INFO;
    if (strcmp(value, "CHARACTER") == 0) return PANE_CHARACTER;
    if (strcmp(value, "LOG") == 0) return PANE_LOG;
    if (strcmp(value, "MONSTERS") == 0) return PANE_MONSTERS;
    return PANE_MAIN;
}

static const char* pane_placement_to_string(enum pane_placement where)
{
    switch (where) {
        case PLACE_BOTTOM: return "BOTTOM";
        case PLACE_RIGHT: return "RIGHT";
        default: return "RIGHT";
    }
}

static enum pane_placement parse_pane_placement(const char* value)
{
    if (!value)
        return PLACE_RIGHT;
    if (strcmp(value, "BOTTOM") == 0) return PLACE_BOTTOM;
    if (strcmp(value, "RIGHT") == 0) return PLACE_RIGHT;
    return PLACE_RIGHT;
}

static char* read_file_contents(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f) {
        log_debug("Could not open JSON file: %s", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        log_error("Failed to allocate memory for JSON file");
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);
    
    return content;
}

void sdl_config_load(const char* filename, struct sdl_config* config, 
                     struct pane_config* pane_configs, int* pane_count, int max_panes)
{
    log_info("Loading SDL configuration from: %s", filename);
    
    char* content = read_file_contents(filename);
    if (!content) {
        log_debug("Failed to read config file, using defaults");
        return;
    }
    
    log_debug("Config file content length: %zu bytes", strlen(content));
    
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            log_error("JSON parse error before: %s", error_ptr);
        } else {
            log_error("JSON parse error (no error pointer available)");
        }
        return;
    }
    
    log_debug("JSON parsed successfully");
    
    // Parse SDL settings
    cJSON* sdl = cJSON_GetObjectItemCaseSensitive(root, "sdl");
    if (cJSON_IsObject(sdl)) {
        log_debug("Found 'sdl' object in JSON");
        cJSON* item;
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "mainViewScale");
        if (cJSON_IsNumber(item)) {
            config->main_view_scale = item->valueint;
            log_debug("Loaded mainViewScale: %d", config->main_view_scale);
        } else {
            log_warn("mainViewScale not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "auxViewFontSize");
        if (cJSON_IsNumber(item)) {
            config->aux_view_font_size = item->valueint;
            log_debug("Loaded auxViewFontSize: %d", config->aux_view_font_size);
        } else {
            log_warn("auxViewFontSize not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "margin");
        if (cJSON_IsNumber(item)) {
            config->margin = item->valueint;
            log_debug("Loaded margin: %d", config->margin);
        } else {
            log_warn("margin not found or not a number");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "fullscreen");
        if (cJSON_IsBool(item)) {
            config->fullscreen = cJSON_IsTrue(item);
            log_debug("Loaded fullscreen: %s", config->fullscreen ? "true" : "false");
        } else {
            log_warn("fullscreen not found or not a boolean");
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "tiles");
        if (cJSON_IsBool(item)) {
            config->tiles = cJSON_IsTrue(item);
            log_debug("Loaded tiles: %s", config->tiles ? "true" : "false");
        } else {
            log_warn("tiles not found or not a boolean");
        }
        
        // Window position and size for windowed mode
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowX");
        if (cJSON_IsNumber(item)) {
            config->window_x = item->valueint;
            log_debug("Loaded windowX: %d", config->window_x);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowY");
        if (cJSON_IsNumber(item)) {
            config->window_y = item->valueint;
            log_debug("Loaded windowY: %d", config->window_y);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowWidth");
        if (cJSON_IsNumber(item)) {
            config->window_width = item->valueint;
            log_debug("Loaded windowWidth: %d", config->window_width);
        }
        
        item = cJSON_GetObjectItemCaseSensitive(sdl, "windowHeight");
        if (cJSON_IsNumber(item)) {
            config->window_height = item->valueint;
            log_debug("Loaded windowHeight: %d", config->window_height);
        }
    } else {
        log_warn("'sdl' object not found in JSON");
    }
    
    // Parse pane configurations
    cJSON* panes = cJSON_GetObjectItemCaseSensitive(root, "panes");
    if (cJSON_IsArray(panes)) {
        int count = 0;
        cJSON* pane_item = NULL;
        int array_size = cJSON_GetArraySize(panes);
        log_debug("Found 'panes' array with %d items", array_size);
        
        cJSON_ArrayForEach(pane_item, panes) {
            if (count >= max_panes) {
                log_warn("Too many panes in config, maximum is %d", max_panes);
                break;
            }
            
            struct pane_config* pc = &pane_configs[count];
            
            cJSON* type = cJSON_GetObjectItemCaseSensitive(pane_item, "type");
            if (cJSON_IsString(type)) {
                pc->pane = parse_pane_type(type->valuestring);
                log_debug("Pane %d: type=%s", count, type->valuestring);
            }
            
            cJSON* where = cJSON_GetObjectItemCaseSensitive(pane_item, "where");
            if (cJSON_IsString(where)) {
                pc->where = parse_pane_placement(where->valuestring);
                log_debug("Pane %d: where=%s", count, where->valuestring);
            }
            
            cJSON* rows = cJSON_GetObjectItemCaseSensitive(pane_item, "rows");
            if (cJSON_IsNumber(rows)) {
                pc->rect.rows = rows->valueint;
                log_debug("Pane %d: rows=%d", count, pc->rect.rows);
            }
            
            cJSON* cols = cJSON_GetObjectItemCaseSensitive(pane_item, "cols");
            if (cJSON_IsNumber(cols)) {
                pc->rect.cols = cols->valueint;
                log_debug("Pane %d: cols=%d", count, pc->rect.cols);
            }
            
            cJSON* ratio = cJSON_GetObjectItemCaseSensitive(pane_item, "ratio");
            if (cJSON_IsNumber(ratio)) {
                pc->ratio = (float)ratio->valuedouble;
                log_debug("Pane %d: ratio=%.2f", count, pc->ratio);
            }
            
            count++;
        }
        
        *pane_count = count;
        log_debug("Parsed %d panes from JSON", count);
    } else {
        log_warn("'panes' array not found in JSON");
    }
    
    cJSON_Delete(root);
    log_debug("Configuration loading complete. Total panes: %d", *pane_count);
}

void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct pane_config* pane_configs, int pane_count)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        log_error("Failed to create JSON root object");
        return;
    }
    
    // Create SDL settings object
    cJSON* sdl = cJSON_CreateObject();
    if (!sdl) {
        cJSON_Delete(root);
        log_error("Failed to create SDL settings object");
        return;
    }
    
    cJSON_AddNumberToObject(sdl, "mainViewScale", config->main_view_scale);
    cJSON_AddNumberToObject(sdl, "auxViewFontSize", config->aux_view_font_size);
    cJSON_AddNumberToObject(sdl, "margin", config->margin);
    cJSON_AddBoolToObject(sdl, "fullscreen", config->fullscreen);
    cJSON_AddBoolToObject(sdl, "tiles", config->tiles);
    
    // Save window position and size for windowed mode
    cJSON_AddNumberToObject(sdl, "windowX", config->window_x);
    cJSON_AddNumberToObject(sdl, "windowY", config->window_y);
    cJSON_AddNumberToObject(sdl, "windowWidth", config->window_width);
    cJSON_AddNumberToObject(sdl, "windowHeight", config->window_height);
    
    cJSON_AddItemToObject(root, "sdl", sdl);
    
    // Create panes array
    cJSON* panes = cJSON_CreateArray();
    if (!panes) {
        cJSON_Delete(root);
        log_error("Failed to create panes array");
        return;
    }
    
    for (int i = 0; i < pane_count; i++) {
        const struct pane_config* pc = &pane_configs[i];
        
        cJSON* pane = cJSON_CreateObject();
        if (!pane) {
            continue;
        }
        
        cJSON_AddStringToObject(pane, "type", pane_type_to_string(pc->pane));
        cJSON_AddStringToObject(pane, "where", pane_placement_to_string(pc->where));
        
        if (pc->rect.rows > 0) {
            cJSON_AddNumberToObject(pane, "rows", pc->rect.rows);
        }
        
        if (pc->rect.cols > 0) {
            cJSON_AddNumberToObject(pane, "cols", pc->rect.cols);
        }
        
        if (pc->ratio > 0.0f) {
            cJSON_AddNumberToObject(pane, "ratio", pc->ratio);
        }
        
        cJSON_AddItemToArray(panes, pane);
    }
    
    cJSON_AddItemToObject(root, "panes", panes);
    
    // Print to string and write to file
    char* json_string = cJSON_Print(root);
    if (!json_string) {
        cJSON_Delete(root);
        log_error("Failed to print JSON");
        return;
    }
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        log_error("Could not write JSON file: %s", filename);
        cJSON_free(json_string);
        cJSON_Delete(root);
        return;
    }
    
    fprintf(f, "%s\n", json_string);
    fclose(f);
    cJSON_free(json_string);
    cJSON_Delete(root);
    
    log_info("Saved SDL configuration to: %s", filename);
}

void sdl_config_set_defaults(struct sdl_config* config)
{
    config->main_view_scale = 1;
    config->aux_view_font_size = 18;
    config->margin = 4;
    config->fullscreen = true;
    config->tiles = true;
    
    // Default window position and size (will be overridden by actual screen size)
    config->window_x = -1;  // -1 means centered
    config->window_y = -1;  // -1 means centered
    config->window_width = 0;  // 0 means use default calculation
    config->window_height = 0; // 0 means use default calculation
}

void sdl_config_apply_cmdline(struct sdl_config* config, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--scale") == 0) {
            if (argc > i + 1) {
                const char* scale_str = argv[++i];
                int scale = atoi(scale_str);
                if (scale > 0) {
                    config->main_view_scale = scale;
                    log_info("Command line: main view scale set to %d", scale);
                }
            }
        } else if (strcmp(argv[i], "--ascii") == 0) {
            config->tiles = false;
            log_info("Command line: ASCII mode enabled");
        } else if (strcmp(argv[i], "--windowed") == 0) {
            config->fullscreen = false;
            log_info("Command line: windowed mode enabled");
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            config->fullscreen = true;
            log_info("Command line: fullscreen mode enabled");
        } else if (strcmp(argv[i], "--tiles") == 0) {
            config->tiles = true;
            log_info("Command line: tiles mode enabled");
        } else if (strcmp(argv[i], "--font-size") == 0) {
            if (argc > i + 1) {
                const char* size_str = argv[++i];
                int size = atoi(size_str);
                if (size > 0) {
                    config->aux_view_font_size = size;
                    log_info("Command line: auxiliary view font size set to %d", size);
                }
            }
        } else if (strcmp(argv[i], "--margin") == 0) {
            if (argc > i + 1) {
                const char* margin_str = argv[++i];
                int margin = atoi(margin_str);
                if (margin >= 0) {
                    config->margin = margin;
                    log_info("Command line: margin set to %d", margin);
                }
            }
        }
    }
}
