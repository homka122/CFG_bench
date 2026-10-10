#pragma once

#include "rsm.h"
#include "symbol_list.h"

#include <stddef.h>

// Build an RSM from the FLPQ_Data .rsm text format, transition-system style:
//
//   start: S              start box, "S" when omitted
//   [box S]
//   start: 0              start state of the box
//   final: 1, 2           final states, separated by commas or spaces
//   0 --a--> 1            transition; a label that names a box is a nonterminal, any other label is a terminal
//   p_i --S--> q_i
//
// Blank lines and lines starting with "#" are skipped. States are not declared: a box consists of its
// start state, its final states and the states of its transitions.
//
// Tokens ending with "_i" (labels and states) are indexed: a line with an indexed token is repeated
// for every block 0..block_count-1 with "_i" replaced by "_<block>", like symbol_numerate does for
// indexed grammar terminals.
//
// Terminals are added to "terms", which the RSM takes over (see rsm_init). On a format error the
// message names "name" and the line, and NULL is returned.
CFG_RSM *rsm_from_text(const char *text, const char *name, size_t block_count, SymbolList *terms);

// rsm_from_text on the contents of the file at "path"; NULL when the file cannot be read.
CFG_RSM *rsm_from_file(const char *path, size_t block_count, SymbolList *terms);
