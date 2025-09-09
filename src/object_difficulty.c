/* Simple heuristic object difficulty metric for Aule quest.
 * This is intentionally lightweight and can be refined later.
 */
#include "angband.h"

int object_difficulty(object_type* o_ptr)
{
    if (!o_ptr) return 0;
    int diff = 0;

    /* Base on damage dice (offensive potential) */
    diff += o_ptr->dd * o_ptr->ds;

    /* Smithing related numeric bonuses */
    diff += o_ptr->att + o_ptr->evn + o_ptr->dd + o_ptr->ds + o_ptr->pd + o_ptr->ps;
    if (o_ptr->pval > 0) diff += o_ptr->pval;

    /* Each ability adds weight */
    diff += o_ptr->abilities * 5;

    /* Ego / artifact amplify */
    if (o_ptr->name1) diff += 15; /* artifact */
    if (o_ptr->name2) diff += 8;  /* ego */

    /* Cap very large values to avoid overflow */
    if (diff > 255) diff = 255;
    return diff;
}
