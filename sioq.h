

/*
     * Copyright (c) - Erez Zadok
     * Copyright (c)       Charles P. Wright
     * Copyright (c) - Josef 'Jeff' Sipek
     * Copyright (c)       Junjiro Okajima
     * Copyright (c)       David P. Quigley
     * Copyright (c) - Stony Brook University
     * Copyright (c) - The Research Foundation of SUNY
     *
    * This program is free software; you can redistribute it and/or modify
    * it under the terms of the GNU General Public License version  as
    * published by the Free Software Foundation.
    */
   
#ifndef _SIOQ_H
#define _SIOQ_H
 

 
struct deletewh_args {
	struct unionfs_dir_state *namelist;
        struct dentry *dentry;
        int bindex;
};
   
struct is_opaque_args {
        struct dentry *dentry;
};
   
struct create_args {
       	struct inode *parent;
        struct dentry *dentry;
       	umode_t mode;
        struct nameidata *nd;
};
   
struct mkdir_args {
        struct inode *parent;
        struct dentry *dentry;
        umode_t mode;
};
   
struct mknod_args {
        struct inode *parent;
        struct dentry *dentry;
        umode_t mode;
        dev_t dev;
};
   
struct symlink_args {
        struct inode *parent;
        struct dentry *dentry;
        char *symbuf;
};
   
struct unlink_args {
        struct inode *parent;
        struct dentry *dentry;
};
   
   
struct sioq_args {
        struct completion comp;
        struct work_struct work;
        int err;
        void *ret;
   
        union {
		struct deletewh_args deletewh;
                struct is_opaque_args is_opaque;
                struct create_args create;
                struct mkdir_args mkdir;
                struct mknod_args mknod;
                struct symlink_args symlink;
                struct unlink_args unlink;
	};
};
   
/* Extern definitions for SIOQ functions */
extern int __init init_sioq(void);
extern void stop_sioq(void);
extern void run_sioq(work_func_t func, struct sioq_args *args);
   
/* Extern definitions for our privilege escalation helpers */
extern void __u2fs_create(struct work_struct *work);
extern void __u2fs_mkdir(struct work_struct *work);
extern void __u2fs_mknod(struct work_struct *work);
extern void __u2fs_symlink(struct work_struct *work);
extern void __u2fs_unlink(struct work_struct *work);
extern void __delete_whiteouts(struct work_struct *work);
extern void __is_opaque_dir(struct work_struct *work);
   
#endif /* not _SIOQ_H */
