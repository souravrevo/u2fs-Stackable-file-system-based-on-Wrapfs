

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

#ifndef _U2FS_H_
#define _U2FS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "sioq.h"
//#include "utils.h"
/* the file system name */
#define U2FS_NAME "u2fs"

/* gi u2fs root inode number */
#define U2FS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

//u2fs mod start
#define LEFT 0
#define RIGHT 1
#define NEGATIVE_DENTRY 2
#define MAXRDCOOKIE (0xfff)
#define RDOFFBITS 20
#define DIREOF (0xfffff)
#define NORMAL 10
#define NEG_LEFT 11
#define NEG_RIGHT 12
//u2fs mod end

/* operations vectors defined in specific files */
extern const struct file_operations u2fs_main_fops;
extern const struct file_operations u2fs_dir_fops;
extern const struct inode_operations u2fs_main_iops;
extern const struct inode_operations u2fs_dir_iops;
extern const struct inode_operations u2fs_symlink_iops;
extern const struct super_operations u2fs_sops;
extern const struct dentry_operations u2fs_dops;
extern const struct address_space_operations u2fs_aops, u2fs_dummy_aops;
extern const struct vm_operations_struct u2fs_vm_ops;

extern int u2fs_init_inode_cache(void);
extern void u2fs_destroy_inode_cache(void);
extern int u2fs_init_dentry_cache(void);
extern void u2fs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern struct dentry *u2fs_lookup(struct inode *dir, struct dentry *dentry,
				    struct nameidata *nd);
extern struct inode *u2fs_iget(struct super_block *sb,
				 struct inode *lower_inode, int idx);
//u2fs operations
extern struct inode *u2fs_iget1(struct super_block *sb,
                                 struct inode *lower_inodel,  struct inode *lower_inoder);

extern struct dentry *create_parents(struct inode *dir, struct dentry *dentry,
                                const char *name, int idx);
extern int copyup_dentry(struct inode *dir, struct dentry *dentry, const char *name, struct file **copyup_file);
extern int copyup_file(struct inode *dir, struct file *file); 
extern char *alloc_whname(const char *name, int len);
extern int create_empty_file(struct inode *dir, struct dentry *dentry, const char *name);
extern int create_empty_dir(struct inode *dir, struct dentry *dentry, const char *name);
extern int create_dir_whiteout(struct inode *dir, struct dentry *dentry, const char *name);

extern int  __u2fs_rename(struct inode *old_dir, struct dentry *old_dentry,
                               struct dentry *old_parent,
                               struct inode *new_dir, struct dentry *new_dentry,
                               struct dentry *new_parent, int idx);


extern struct dentry *lookup_whiteout(const char *name, struct dentry *lower_parent);
extern int create_whiteout(struct dentry *dentry);
enum unionfs_dentry_lock_class {
	U2FS_DMUTEX_NORMAL,
        U2FS_DMUTEX_ROOT,
        U2FS_DMUTEX_PARENT,
        U2FS_DMUTEX_CHILD,
        U2FS_DMUTEX_WHITEOUT,
        U2FS_DMUTEX_REVAL_PARENT, /* for file/dentry revalidate */
        U2FS_DMUTEX_REVAL_CHILD,   /* for file/dentry revalidate */
};

extern void dput_lower_dentry(struct dentry *dentry);
extern bool  is_validname(const char *name);
extern int u2fs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path, int idx);

/* file private data */
struct u2fs_file_info {
	struct file *lower_file[2];
	const struct vm_operations_struct *lower_vm_ops;
	struct u2fs_dir_state *rdstate;
};
/* u2fs inode data in memory */
struct u2fs_inode_info {
	struct inode *lower_inode[2];
	struct inode vfs_inode;
};

/* u2fs dentry data in memory */
struct u2fs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path[2];
};

/* u2fs super-block data in memory */
struct u2fs_sb_info {
	struct super_block *lower_sb[2];
};

struct u2fs_data {
             struct super_block *sb; /* lower super_block */
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * u2fs_inode_info structure, U2FS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct u2fs_inode_info *U2FS_I(const struct inode *inode)
{
	return container_of(inode, struct u2fs_inode_info, vfs_inode);
}

/* dentry to private data */
#define U2FS_D(dent) ((struct u2fs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define U2FS_SB(super) ((struct u2fs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define U2FS_F(file) ((struct u2fs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *u2fs_lower_file_left(const struct file *f)
{
	return U2FS_F(f)->lower_file[LEFT]; 
}

static inline struct file *u2fs_lower_file_right(const struct file *f)
{
        return U2FS_F(f)->lower_file[RIGHT];
}


static inline void u2fs_set_lower_file(struct file *f, struct file *val, int idx)
{
	if(idx == LEFT)
 		U2FS_F(f)->lower_file[LEFT] = val;
	else if(idx == RIGHT)	
		U2FS_F(f)->lower_file[RIGHT] = val;
}

/* inode to lower inode. */
static inline struct inode *u2fs_lower_inode_left(const struct inode *i)
{
	return U2FS_I(i)->lower_inode[LEFT];
}

static inline struct inode *u2fs_lower_inode_right(const struct inode *i)
{
        return U2FS_I(i)->lower_inode[RIGHT];
}


static inline void u2fs_set_lower_inode(struct inode *i, struct inode *val, int idx)
{
	if(idx == LEFT)
		U2FS_I(i)->lower_inode[LEFT] = val;
	else if(idx == RIGHT)
 		U2FS_I(i)->lower_inode[RIGHT] = val;
}

/* superblock to lower superblock */
static inline struct super_block *u2fs_lower_super_left(
	const struct super_block *sb)
{
		return U2FS_SB(sb)->lower_sb[LEFT];
}

static inline struct super_block *u2fs_lower_super_right(
        const struct super_block *sb)
{
                return U2FS_SB(sb)->lower_sb[RIGHT];
}


static inline void u2fs_set_lower_super(struct super_block *sb,
					  struct super_block *val, int idx)
{
	if(idx == LEFT)
		U2FS_SB(sb)->lower_sb[LEFT] = val;
	else if(idx == RIGHT)
		U2FS_SB(sb)->lower_sb[RIGHT] = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void u2fs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path, int idx)
{
	spin_lock(&U2FS_D(dent)->lock);
	if(idx == LEFT)
		pathcpy(lower_path, &U2FS_D(dent)->lower_path[LEFT]);
	else if(idx == RIGHT)
		pathcpy(lower_path, &U2FS_D(dent)->lower_path[RIGHT]);
	path_get(lower_path);
	spin_unlock(&U2FS_D(dent)->lock);
	return;
}
static inline void u2fs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
	return;
}
static inline void u2fs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path, int idx)
{
	spin_lock(&U2FS_D(dent)->lock);
	if(idx == LEFT)
		pathcpy(&U2FS_D(dent)->lower_path[LEFT], lower_path);
	else if(idx == RIGHT)
		pathcpy(&U2FS_D(dent)->lower_path[RIGHT], lower_path);
	spin_unlock(&U2FS_D(dent)->lock);
	return;
}
static inline void u2fs_reset_lower_path(const struct dentry *dent, int idx)
{
	spin_lock(&U2FS_D(dent)->lock);
	if(idx == LEFT){
		U2FS_D(dent)->lower_path[LEFT].dentry = NULL;
		U2FS_D(dent)->lower_path[LEFT].mnt = NULL;
	}
	else if(idx == RIGHT){
		U2FS_D(dent)->lower_path[RIGHT].dentry = NULL;
                U2FS_D(dent)->lower_path[RIGHT].mnt = NULL;
	}	
	spin_unlock(&U2FS_D(dent)->lock);
	return;
}
static inline void u2fs_put_reset_lower_path(const struct dentry *dent, int idx)
{
	struct path lower_path;
	spin_lock(&U2FS_D(dent)->lock);
	if(idx == LEFT){
		pathcpy(&lower_path, &U2FS_D(dent)->lower_path[LEFT]);
		U2FS_D(dent)->lower_path[LEFT].dentry = NULL;
		U2FS_D(dent)->lower_path[LEFT].mnt = NULL;
	}
	else if(idx == RIGHT){
		pathcpy(&lower_path, &U2FS_D(dent)->lower_path[RIGHT]);
                U2FS_D(dent)->lower_path[RIGHT].dentry = NULL;
                U2FS_D(dent)->lower_path[RIGHT].mnt = NULL;
	}
	
	spin_unlock(&U2FS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}


//skip file structures and functions

struct u2fs_getdents_callback {
        struct unionfs_dir_state *rdstate;
        void *dirent;
        int entries_written;
        int filldir_called;
        int filldir_error;
        filldir_t filldir;
        struct super_block *sb;
	const unsigned char **names;
	int count;
	int idx;
};

struct u2fs_dir_state {
         unsigned int cookie;    /* the cookie, based off of rdversion */
         unsigned int offset;    /* The entry we have returned. */
         int bindex;
         loff_t dirpos;          /* offset within the lower level directory */
         int size;               /* How big is the hash table? */
         int hashentries;        /* How many entries have been inserted? */
         unsigned long access;
 
         /* This cache list is used when the inode keeps us around. */
        struct list_head cache; 
        struct list_head list[0];
};

static inline off_t rdstate2offset(struct u2fs_dir_state *buf)
{
        off_t tmp;
 
        tmp = ((buf->cookie & MAXRDCOOKIE) << RDOFFBITS)
                 | (buf->offset & DIREOF);
        return tmp;
}
#endif	/* not _U2FS_H_ */
