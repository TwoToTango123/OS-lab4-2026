#include "text.h"

void reverse_text(char *text, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        char tmp = text[i];
        text[i] = text[len - i - 1];
        text[len - i - 1] = tmp;
    }
}
