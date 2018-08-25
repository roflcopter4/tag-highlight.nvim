#include "util/util.h"

#include "data.h"
#include "highlight.h"

#define INIT_STRINGS 8192

extern struct backups backup_pointers;

typedef bool (*cmp_f)(char, bool);

static void do_tokenize(b_list *list, char *vimbuf, cmp_f check);
static void tokenize_vim(b_list *list, char *vimbuf, cmp_f check);
static char *strsep_f(char **stringp, cmp_f check);
static bool c_func   (char ch, bool first);
static bool vim_func (char ch, bool first);


b_list *
tokenize(struct bufdata *bdata, bstring *vimbuf)
{
        b_list *list = b_list_create_alloc(INIT_STRINGS);

        switch (bdata->ft->id) {
        case FT_VIM: tokenize_vim(list, BS(vimbuf), vim_func); break;
        default:     do_tokenize(list, BS(vimbuf), c_func);  break;
        }

        return list;
}


static void
do_tokenize(b_list *list, char *vimbuf, cmp_f check)
{
        char *tok = NULL;

        while ((tok = strsep_f(&vimbuf, check)) != NULL) {
               if (!tok[0])
                       continue;
               bstring *tmp = b_refblk(tok, vimbuf - tok - 1);
               b_list_append(&list, tmp);
        }
}


static char *
strsep_f(char **stringp, cmp_f check)
{
        char *ptr, *tok;
        if ((ptr = tok = *stringp) == NULL)
                return NULL;

        for (bool first = true;;first = false) {
                const char src_ch = *ptr++;
                if (!check(src_ch, first)) {
                        if (src_ch == '\0')
                                ptr = NULL;
                        else
                                *(ptr - 1) = '\0';
                        *stringp = ptr;
                        return tok;
                }
        }
}


static bool c_func(const char ch, const bool first)
{
        return (first) ? (ch == '_' || isalpha(ch))
                       : (ch == '_' || isalnum(ch));
} 

static bool vim_func(const char ch, const bool first)
{
        return (first) ? (ch == '_' || isalpha(ch))
                       : (ch == '_' || ch == ':' || isalnum(ch));
} 


//===============================================================================


static void
tokenize_vim(b_list *list, char *vimbuf, cmp_f check)
{
        char *tok = NULL, *col = NULL;

        while ((tok = strsep_f(&vimbuf, check)) != NULL) {
               if (!tok[0])
                       continue;
               bstring *tmp = b_refblk(tok, vimbuf - tok - 1);
               b_list_append(&list, tmp);

               if ((col = strchr(tok, ':'))) {
                       if (!(vimbuf - col))
                               continue;
                       tmp = b_refblk(tok, vimbuf - col);
                       b_list_append(&list, tmp);
               }
        }
}
