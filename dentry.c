/*
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "wrapfs.h"

/*
 * returns: -ERRNO if error (returned to user)
 *          0: tell VFS to invalidate dentry
 *          1: dentry is valid
 */
static int u2fs_d_revalidate(struct dentry *dentry, struct nameidata *nd)
{
/*revalidate for efficiency is changed to revalidate both LB and RB */

	struct path lower_path, saved_path;
	struct dentry *lower_dentry;
	int err = 1;

	if (nd && nd->flags & LOOKUP_RCU)
		return -ECHILD;

	u2fs_get_lower_path(dentry, &lower_path, LEFT);
	if(lower_path.dentry != NULL){
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_op || !lower_dentry->d_op->d_revalidate)
			goto right;
		pathcpy(&saved_path, &nd->path);
		pathcpy(&nd->path, &lower_path);
		err = lower_dentry->d_op->d_revalidate(lower_dentry, nd);
		pathcpy(&nd->path, &saved_path);
	}
right:
	u2fs_get_lower_path(dentry, &lower_path, RIGHT);
        if(lower_path.dentry != NULL){
                lower_dentry = lower_path.dentry;
                if (!lower_dentry->d_op || !lower_dentry->d_op->d_revalidate)
                        goto out;
                pathcpy(&saved_path, &nd->path);
                pathcpy(&nd->path, &lower_path);
                err = lower_dentry->d_op->d_revalidate(lower_dentry, nd);
                pathcpy(&nd->path, &saved_path);
        }

out:
	u2fs_put_lower_path(dentry, &lower_path);
	return err;
}

static void u2fs_d_release(struct dentry *dentry)
{
//Releases LB and RB as given	
	if(U2FS_D(dentry)->lower_path[LEFT].dentry != NULL)
		u2fs_put_reset_lower_path(dentry, LEFT);
	if(U2FS_D(dentry)->lower_path[RIGHT].dentry != NULL)
		u2fs_put_reset_lower_path(dentry, RIGHT);
	free_dentry_private_data(dentry);
	return;
}

const struct dentry_operations u2fs_dops = {
	.d_revalidate	= u2fs_d_revalidate,
	.d_release	= u2fs_d_release,
};
