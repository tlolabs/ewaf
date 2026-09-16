#ifndef EWAF_H
#define EWAF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ABI v1. Borrow request bytes for this call; free the response exactly once.
 */
char *ewaf_request(const uint8_t *input, size_t length);
void ewaf_string_free(char *response);
/* Dates use Gregorian ordinals (0001-01-01 = 1). Zero indicates invalid input.
 */
int32_t ewaf_date_ordinal(int32_t year, uint32_t month, uint32_t day);
int32_t ewaf_date_add(int32_t ordinal, int32_t days);
int32_t ewaf_date_component(int32_t ordinal, uint32_t component);
bool ewaf_date_name(int32_t ordinal, uint8_t *output);
int32_t ewaf_date_parse(const uint8_t *input, size_t length);
#ifdef __cplusplus
}
#endif
#endif
