#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/*
    ORIGINAL AUTHOR
	Erik Kay

    MODIFIED BY
	[1994-07-09] by Robocoder - modified to use AVL tree
	[2001-06-26] by Annihilator <annihilatro@muds.net>
*/

struct userid_s {
    shared_str_t name;
};

extern userid_t *backbone_uid;
extern userid_t *root_uid;

userid_t *add_uid(const char *name);
userid_t *set_root_uid(const char *name);
userid_t *set_backbone_uid(const char *name);

void init_uids(void);
void deinit_uids(void);

#ifdef __cplusplus
}
#endif
