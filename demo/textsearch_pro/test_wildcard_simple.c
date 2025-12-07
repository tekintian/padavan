/*
 * Simple wildcard pattern test
 */

#include "textsearch.h"
#include <stdio.h>
#include <string.h>

extern void init_wildcard(void);
extern void exit_wildcard(void);

int main(void)
{
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    const char *text = "Hello World! This is a test string for wildcard matching.";
    
    printf("Wildcard Pattern Test\n");
    printf("====================\n");
    printf("Text: \"%s\"\n\n", text);
    
    init_wildcard();
    
    struct {
        const char *pattern;
        const char *description;
    } tests[] = {
        {"*", "Match everything"},
        {"Hello*", "Starts with Hello"},
        {"*ing", "Ends with ing"},
        {"*is*", "Contains is"},
        {"H?llo", "H?llo (single char wildcard)"},
        {"Th?s ?s *", "Multiple wildcards"},
        {"Hello World!", "Exact match"},
        {"Hello World!?", "Exact match + one more"},
        {"NoMatch*", "No match"},
        {NULL, NULL}
    };
    
    for (int i = 0; tests[i].pattern; i++) {
        printf("Pattern: \"%s\" - %s\n", tests[i].pattern, tests[i].description);
        
        conf = textsearch_prepare("wildcard", tests[i].pattern, 
                                strlen(tests[i].pattern), 0, TS_WILDCARD);
        if (!conf) {
            printf("  ERROR: Failed to prepare config\n");
            continue;
        }
        
        pos = textsearch_find_continuous(conf, &state, text, strlen(text));
        if (pos != UINT_MAX) {
            printf("  MATCH at position %u: \"%.20s\"\n", pos, text + pos);
        } else {
            printf("  NO MATCH\n");
        }
        
        textsearch_destroy(conf);
        printf("\n");
    }
    
    exit_wildcard();
    return 0;
}