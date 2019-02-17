/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "log.h"
#include "fifo.h"

#include "config_file.h"

#define CFG_NODE_KEY_LEN 256
#define CFG_NODE_VAL_LEN 256

#define CFG_FILE_NAME "wash.cfg"

struct cfg_node {
    struct fifo_node list_node;
    char key[CFG_NODE_KEY_LEN];
    char val[CFG_NODE_VAL_LEN];
};

enum cfg_parse_state {
    CFG_PARSE_PRE_KEY,
    CFG_PARSE_KEY,
    CFG_PARSE_PRE_VAL,
    CFG_PARSE_VAL,
    CFG_PARSE_POST_VAL,
    CFG_PARSE_ERROR
};

static struct cfg_state {
    enum cfg_parse_state state;
    unsigned key_len, val_len;
    char key[CFG_NODE_KEY_LEN];
    char val[CFG_NODE_VAL_LEN];
    unsigned line_count;
    struct fifo_head cfg_nodes;
    bool in_comment;
} cfg_state;

static void cfg_add_entry(void);
static void cfg_handle_newline(void);
static int cfg_parse_bool(char const *val, bool *outp);

void cfg_init(void) {
    memset(&cfg_state, 0, sizeof(cfg_state));
    cfg_state.state = CFG_PARSE_PRE_KEY;

    fifo_init(&cfg_state.cfg_nodes);

    FILE *cfg_file = fopen(CFG_FILE_NAME, "r");
    if (cfg_file) {
        LOG_INFO("Parsing configuration file %s\n", CFG_FILE_NAME);
        for (;;) {
            int ch = fgetc(cfg_file);
            if (ch == EOF)
                break;
            cfg_put_char(ch);
        }
        cfg_put_char('\n'); // in case the last line doesn't end with newline
        fclose(cfg_file);
    } else {
        LOG_INFO("Unable to open %s; does it even exist?\n", CFG_FILE_NAME);
    }
}

void cfg_cleanup(void) {
    struct fifo_node *curs;

    while ((curs = fifo_pop(&cfg_state.cfg_nodes)) != NULL) {
        struct cfg_node *node = &FIFO_DEREF(curs, struct cfg_node, list_node);
        free(node);
    }
}

void cfg_put_char(char ch) {
    /*
     * special case - a null terminator counts as a newline so that any data
     * which does not end in a newline can be flushed.
     */
    if (ch == '\0')
        ch = '\n';

    /*
     * Very simple preprocessor - replace comments with whitespace and
     * otherwise don't modify the parser state
     */
    if (ch == ';')
        cfg_state.in_comment = true;
    if (cfg_state.in_comment) {
        if (ch == '\n')
            cfg_state.in_comment = false;
        else
            ch = ' ';
    }

    switch (cfg_state.state) {
    case CFG_PARSE_PRE_KEY:
        if (ch == '\n') {
            cfg_handle_newline();
        } else if (!isspace(ch)) {
            cfg_state.state = CFG_PARSE_KEY;
            cfg_state.key_len = 1;
            cfg_state.key[0] = ch;
        }
        break;
    case CFG_PARSE_KEY:
        if (ch == '\n') {
            LOG_ERROR("*** CFG ERROR INCOMPLETE LINE %u ***\n", cfg_state.line_count);
            cfg_handle_newline();
        } else if (isspace(ch)) {
            cfg_state.state = CFG_PARSE_PRE_VAL;
            cfg_state.key[cfg_state.key_len] = '\0';
        } else if (cfg_state.key_len < CFG_NODE_KEY_LEN - 1) {
            cfg_state.key[cfg_state.key_len++] = ch;
        } else {
            LOG_WARN("CFG file dropped char from line %u; key length is "
                     "limited to %u characters\n",
                     cfg_state.line_count, CFG_NODE_KEY_LEN - 1);
        }
        break;
    case CFG_PARSE_PRE_VAL:
        if (ch == '\n') {
            LOG_ERROR("*** CFG ERROR INCOMPLETE LINE %u ***\n", cfg_state.line_count);
            cfg_handle_newline();
        } else if (!isspace(ch)) {
            cfg_state.state = CFG_PARSE_VAL;
            cfg_state.val_len = 1;
            cfg_state.val[0] = ch;
        }
        break;
    case CFG_PARSE_VAL:
        if (ch == '\n') {
            cfg_state.val[cfg_state.val_len] = '\0';
            cfg_add_entry();
            cfg_handle_newline();
        } else if (isspace(ch)) {
            cfg_state.state = CFG_PARSE_POST_VAL;
            cfg_state.val[cfg_state.val_len] = '\0';
        } else if (cfg_state.val_len < CFG_NODE_VAL_LEN - 1) {
            cfg_state.val[cfg_state.val_len++] = ch;
        } else {
            LOG_WARN("CFG file dropped char from line %u; value length is "
                     "limited to %u characters\n",
                     cfg_state.line_count, CFG_NODE_VAL_LEN - 1);
        }
        break;
    case CFG_PARSE_POST_VAL:
        if (ch == '\n') {
            cfg_add_entry();
            cfg_handle_newline();
        } else if (!isspace(ch)) {
            cfg_state.state = CFG_PARSE_ERROR;
            LOG_ERROR("*** CFG ERROR INVALID DATA LINE %u ***\n", cfg_state.line_count);
        }
        break;
    default:
    case CFG_PARSE_ERROR:
        if (ch == '\n')
            cfg_handle_newline();
        break;
    }
}

static void cfg_add_entry(void) {
    struct cfg_node *dst_node = NULL;
    struct fifo_node *curs;

    FIFO_FOREACH(cfg_state.cfg_nodes, curs) {
        struct cfg_node *node = &FIFO_DEREF(curs, struct cfg_node, list_node);
        if (strcmp(node->key, cfg_state.key) == 0) {
            dst_node = node;
            break;
        }
    }

    if (dst_node) {
        LOG_INFO("CFG overwriting existing config key \"%s\" at line %u\n",
                 cfg_state.key, cfg_state.line_count);
    } else {
        LOG_INFO("CFG allocating new config key \"%s\" at line %u\n",
                 cfg_state.key, cfg_state.line_count);
        dst_node = (struct cfg_node*)malloc(sizeof(struct cfg_node));
        memcpy(dst_node->key, cfg_state.key, sizeof(dst_node->key));
        fifo_push(&cfg_state.cfg_nodes, &dst_node->list_node);
    }

    if (dst_node)
        memcpy(dst_node->val, cfg_state.val, sizeof(dst_node->val));
    else
        LOG_ERROR("CFG file dropped line %u due to failed node allocation\n", cfg_state.line_count);
}

static void cfg_handle_newline(void) {
    cfg_state.state = CFG_PARSE_PRE_KEY;
    cfg_state.key_len = 0;
    cfg_state.val_len = 0;
    cfg_state.line_count++;
}

char const *cfg_get_node(char const *key) {
    struct fifo_node *curs;

    FIFO_FOREACH(cfg_state.cfg_nodes, curs) {
        struct cfg_node *node = &FIFO_DEREF(curs, struct cfg_node, list_node);
        if (strcmp(node->key, key) == 0)
            return node->val;
    }

    return NULL;
}

static int cfg_parse_bool(char const *valstr, bool *outp) {
    if (strcmp(valstr, "true") == 0 || strcmp(valstr, "1") == 0) {
        *outp = true;
        return 0;
    } else if (strcmp(valstr, "false") == 0 || strcmp(valstr, "0") == 0) {
        *outp = false;
        return 0;
    }
    return -1;
}

int cfg_get_bool(char const *key, bool *outp) {
    char const *nodestr = cfg_get_node(key);
    if (nodestr) {
        int success = cfg_parse_bool(nodestr, outp);
        if (success != 0)
            LOG_ERROR("error parsing config node \"%s\"\n", key);
        return success;
    }
    return -1;
}
