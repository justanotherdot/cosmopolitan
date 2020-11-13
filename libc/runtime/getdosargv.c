/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ This program is free software; you can redistribute it and/or modify         │
│ it under the terms of the GNU General Public License as published by         │
│ the Free Software Foundation; version 2 of the License.                      │
│                                                                              │
│ This program is distributed in the hope that it will be useful, but          │
│ WITHOUT ANY WARRANTY; without even the implied warranty of                   │
│ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU             │
│ General Public License for more details.                                     │
│                                                                              │
│ You should have received a copy of the GNU General Public License            │
│ along with this program; if not, write to the Free Software                  │
│ Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA                │
│ 02110-1301 USA                                                               │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "libc/bits/bits.h"
#include "libc/bits/pushpop.h"
#include "libc/bits/safemacros.h"
#include "libc/runtime/internal.h"
#include "libc/str/appendchar.h"
#include "libc/str/str.h"
#include "libc/str/tpenc.h"
#include "libc/str/utf16.h"

struct DosArgv {
  const char16_t *s;
  char *p;
  char *pe;
  wint_t wc;
};

static textwindows void AppendDosArgv(struct DosArgv *st, wint_t wc) {
  AppendChar(&st->p, st->pe, wc);
}

/**
 * Tokenizes and transcodes Windows NT CLI args, thus avoiding
 * CommandLineToArgv() schlepping in forty megs of dependencies.
 *
 * @param s is the command line string provided by the executive
 * @param buf is where we'll store double-NUL-terminated decoded args
 * @param size is how many bytes are available in buf
 * @param argv is where we'll store the decoded arg pointer array, which
 *     is guaranteed to be NULL-terminated if max>0
 * @param max specifies the item capacity of argv, or 0 to do scanning
 * @return number of args written, excluding the NULL-terminator; or,
 *     if the output buffer wasn't passed, or was too short, then the
 *     number of args that *would* have been written is returned; and
 *     there are currently no failure conditions that would have this
 *     return -1 since it doesn't do system calls
 * @see test/libc/dosarg_test.c
 * @see libc/runtime/ntspawn.c
 * @note kudos to Simon Tatham for figuring out quoting behavior
 */
textwindows int GetDosArgv(const char16_t *cmdline, char *buf, size_t size,
                           char **argv, size_t max) {
  bool inquote;
  size_t i, argc, slashes, quotes;
  struct DosArgv st;
  st.s = cmdline;
  st.p = buf;
  st.pe = buf + size;
  argc = 0;
  st.wc = DecodeNtsUtf16(&st.s);
  while (st.wc) {
    while (st.wc && iswspace(st.wc)) st.wc = DecodeNtsUtf16(&st.s);
    if (!st.wc) break;
    if (++argc < max) {
      argv[argc - 1] = st.p < st.pe ? st.p : NULL;
    }
    inquote = false;
    while (st.wc) {
      if (!inquote && isspace(st.wc)) break;
      if (st.wc == '"' || st.wc == '\\') {
        slashes = 0;
        quotes = 0;
        while (st.wc == '\\') st.wc = DecodeNtsUtf16(&st.s), slashes++;
        while (st.wc == '"') st.wc = DecodeNtsUtf16(&st.s), quotes++;
        if (!quotes) {
          while (slashes--) AppendDosArgv(&st, '\\');
        } else {
          while (slashes >= 2) AppendDosArgv(&st, '\\'), slashes -= 2;
          if (slashes) AppendDosArgv(&st, '"'), quotes--;
          if (quotes > 0) {
            if (!inquote) quotes--;
            for (i = 3; i <= quotes + 1; i += 3) AppendDosArgv(&st, '"');
            inquote = (quotes % 3 == 0);
          }
        }
      } else {
        AppendDosArgv(&st, st.wc);
        st.wc = DecodeNtsUtf16(&st.s);
      }
    }
    AppendDosArgv(&st, '\0');
  }
  AppendDosArgv(&st, '\0');
  if (size) buf[min(st.p - buf, size - 1)] = '\0';
  if (max) argv[min(argc, max - 1)] = NULL;
  return argc;
}
