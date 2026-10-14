// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_H
#define EWAF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ABI v1 C FFI declarations.
 *
 * Ownership and buffer requirements:
 * - ewaf_request(): `input` must point to `length` readable bytes (up to 16 MiB),
 *   or be NULL if length is 0. Returns a dynamically allocated NUL-terminated
 *   JSON C string. The caller assumes ownership and MUST release it exactly once
 *   using ewaf_string_free().
 * - ewaf_string_free(): Frees memory returned by ewaf_request(). Passing NULL is safe.
 *   Double-freeing or passing pointers from other allocators is undefined behavior.
 * - Dates use Gregorian ordinals (0001-01-01 = 1, 9999-12-31 = 3,652,059).
 *   The value 0 is the universal sentinel for invalid dates or failed operations.
 * - ewaf_date_name(): `output` MUST point to a writable buffer of at least 10 bytes.
 *   Writes 10 ASCII characters (MM-DD-YYYY) without a terminating NUL byte.
 *   Returns true if the ordinal is valid and characters were written, false otherwise.
 * - ewaf_date_parse(): `input` must point to `length` readable bytes in MM-DD-YYYY format.
 * - ewaf_date_add(): Adds signed `days` to `ordinal`. Returns resulting ordinal,
 *   or 0 if invalid or outside 0001-01-01..9999-12-31. Safely handles i32::MIN.
 */
char *ewaf_request(const uint8_t *input, size_t length);
void ewaf_string_free(char *response);
int32_t ewaf_date_ordinal(int32_t year, uint32_t month, uint32_t day);
int32_t ewaf_date_add(int32_t ordinal, int32_t days);
int32_t ewaf_date_component(int32_t ordinal, uint32_t component);
bool ewaf_date_name(int32_t ordinal, uint8_t *output);
int32_t ewaf_date_parse(const uint8_t *input, size_t length);
#ifdef __cplusplus
}
#endif
#endif
