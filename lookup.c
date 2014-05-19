
#include "wrapfs.h"


/* The dentry cache is just so we have properly sized dentries */
static struct kmem_cache *u2fs_dentry_cachep;

int u2fs_init_dentry_cache(void)
{
	u2fs_dentry_cachep =
		kmem_cache_create("u2fs_dentry",
				  sizeof(struct u2fs_dentry_info),
				  0, SLAB_RECLAIM_ACCOUNT, NULL);

	return u2fs_dentry_cachep ? 0 : -ENOMEM;
}

void u2fs_destroy_dentry_cache(void)
{
	if (u2fs_dentry_cachep)
		kmem_cache_destroy(u2fs_dentry_cachep);
}

void free_dentry_private_data(struct dentry *dentry)
{
	if (!dentry || !dentry->d_fsdata)
		return;
	kmem_cache_free(u2fs_dentry_cachep, dentry->d_fsdata);
	dentry->d_fsdata = NULL;
}

/* allocate new dentry private data */
int new_dentry_private_data(struct dentry *dentry)
{
	struct u2fs_dentry_info *info = U2FS_D(dentry);

	UDBG;
	/* use zalloc to init dentry_info.lower_path */
	info = kmem_cache_zalloc(u2fs_dentry_cachep, GFP_ATOMIC);
	if (!info)
		return -ENOMEM;

	memset((info)->lower_path, 0 , 2*sizeof(struct path));
	spin_lock_init(&info->lock);
	dentry->d_fsdata = info;

	return 0;
}

static int u2fs_inode_test(struct inode *inode, void *candidate_lower_inode)
{
	struct inode *current_lower_inode = u2fs_lower_inode_left(inode);
	if (current_lower_inode == (struct inode *)candidate_lower_inode)
		return 1; /* found a match */
	else
		return 0; /* no match */
}

static int u2fs_inode_set(struct inode *inode, void *lower_inode)
{
	/* we do actual inode initialization in u2fs_iget */
return 0;
}

//u2fs mod start

struct inode *u2fs_iget1(struct super_block *sb, struct inode *lower_inodel, struct inode *lower_inoder)
{
/*This iget version is called only once when U2FS is mounted */
struct u2fs_inode_info *info;
struct inode *inode; /* the new inode to return */
int err;

inode = iget5_locked(sb, /* our superblock */
		     /*
		      * hashval: we use inode number, but we can
		      * also use "(unsigned long)lower_inode"
		      * instead.
		      */
		     lower_inodel->i_ino, /* hashval */
		     u2fs_inode_test,	/* inode comparison function */
		     u2fs_inode_set, /* inode init function */
			     lower_inodel); /* data passed to test+set fxns */
	

	if (!inode) {
		err = -EACCES;
		iput(lower_inodel);
		return ERR_PTR(err);
	}
	/* if found a cached inode, then just return it */
	if (!(inode->i_state & I_NEW))
		return inode;

	/* initialize new inode */
	info = U2FS_I(inode);

	inode->i_ino = lower_inodel->i_ino;
	if (!igrab(lower_inodel)) {
		err = -ESTALE;
		return ERR_PTR(err);
	}

	u2fs_set_lower_inode(inode, lower_inodel, LEFT);
	u2fs_set_lower_inode(inode, lower_inoder, RIGHT);

	inode->i_version++;

	/* use different set of inode ops for symlinks & directories */
	if (S_ISDIR(lower_inodel->i_mode))
		inode->i_op = &u2fs_dir_iops;
	else if (S_ISLNK(lower_inodel->i_mode))
		inode->i_op = &u2fs_symlink_iops;
	else
		inode->i_op = &u2fs_main_iops;
	

	/* use different set of file ops for directories */
	if (S_ISDIR(lower_inodel->i_mode))
		inode->i_fop = &u2fs_dir_fops;
	else
		inode->i_fop = &u2fs_main_fops;

	inode->i_mapping->a_ops = &u2fs_aops;
	

	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;
	

	/* properly initialize special inodes */
	if (S_ISBLK(lower_inodel->i_mode) || S_ISCHR(lower_inodel->i_mode) ||
	    S_ISFIFO(lower_inodel->i_mode) || S_ISSOCK(lower_inodel->i_mode))
		init_special_inode(inode, lower_inodel->i_mode,
				   lower_inodel->i_rdev);

	/* all well, copy inode attributes */
	fsstack_copy_attr_all(inode, lower_inodel);
	fsstack_copy_inode_size(inode, lower_inodel);

	unlock_new_inode(inode);
	return inode;
}



struct inode *u2fs_iget(struct super_block *sb, struct inode *lower_inode, int idx)
{
/*This function gives inode specific data for LB and RB */
	struct u2fs_inode_info *info;
	struct inode *inode; /* the new inode to return */
	int err;
	int ino = iunique(sb, U2FS_ROOT_INO);

	inode = iget5_locked(sb, /* our superblock */
			     /*
			      * hashval: we use inode number, but we can
			      * also use "(unsigned long)lower_inode"
			      * instead.
			      */
			     ino, /* hashval */
			     u2fs_inode_test,	/* inode comparison function */
			     u2fs_inode_set, /* inode init function */
			     lower_inode); /* data passed to test+set fxns */

	if (!inode) {
		err = -EACCES;
		iput(lower_inode);
		return ERR_PTR(err);
	}
	/* if found a cached inode, then just return it */
	if (!(inode->i_state & I_NEW))
		return inode;

	/* initialize new inode */
	info = U2FS_I(inode);

	inode->i_ino = ino;
	if (!igrab(lower_inode)) {
		err = -ESTALE;
		return ERR_PTR(err);
	}
	if(idx == LEFT)
		u2fs_set_lower_inode(inode, lower_inode, LEFT);
	else if(idx == RIGHT)
		u2fs_set_lower_inode(inode, lower_inode, RIGHT);

	inode->i_version++;

	/* use different set of inode ops for symlinks & directories */

	if (S_ISDIR(lower_inode->i_mode))
		inode->i_op = &u2fs_dir_iops;
	else if (S_ISLNK(lower_inode->i_mode))
		inode->i_op = &u2fs_symlink_iops;
	else
		inode->i_op = &u2fs_main_iops;

	/* use different set of file ops for directories */
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_fop = &u2fs_dir_fops;
	else
		inode->i_fop = &u2fs_main_fops;

	inode->i_mapping->a_ops = &u2fs_aops;
	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;

	/* properly initialize special inodes */
	if (S_ISBLK(lower_inode->i_mode) || S_ISCHR(lower_inode->i_mode) ||
	    S_ISFIFO(lower_inode->i_mode) || S_ISSOCK(lower_inode->i_mode))
		init_special_inode(inode, lower_inode->i_mode,
				   lower_inode->i_rdev);

	/* all well, copy inode attributes */
	fsstack_copy_attr_all(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);
	unlock_new_inode(inode);
	return inode;
}


int u2fs_interpose(struct dentry *dentry, struct super_block *sb,
                     struct path *lower_path, int idx)
{
        int err = 0;
        struct inode *inode = NULL;
        struct inode *lower_inode = NULL;
        struct super_block *lower_sb = NULL;

        lower_inode = lower_path->dentry->d_inode;
	if(idx == LEFT)
	        lower_sb = u2fs_lower_super_left(sb);
	else if(idx == RIGHT)
		lower_sb = u2fs_lower_super_right(sb);

        /* check that the lower file system didn't cross a mount point */
        if (lower_inode->i_sb != lower_sb) {
                err = -EXDEV;
                goto out;
        }
        /*
         * We allocate our new inode below by calling u2fs_iget,
         * which will initialize some of the new inode's fields
         */

        /* inherit lower inode number for u2fs's inode */
	if(idx == LEFT)
        	inode = u2fs_iget(sb, lower_inode, LEFT);
	else if(idx == RIGHT)
		inode = u2fs_iget(sb, lower_inode, RIGHT);

        if (IS_ERR(inode)) {
                err = PTR_ERR(inode);
                goto out;
        }
        d_add(dentry, inode);

out:
        return err;
}

//u2fs mod end

/*
 * Main driver function for u2fs's lookup.
 *
 * Returns: NULL (ok), ERR_PTR if an error occurred.
 * Fills in lower_parent_path with <dentry,mnt> on success.
 */
static struct dentry *__u2fs_lookup(struct dentry *dentry, int flags,
				      struct path *lower_parent_path, int idx, int mode)
{
/*This function is called by u2fs_lookup to set inodes or create negative dentry */
	int err = 0;
	struct vfsmount *lower_dir_mnt = NULL;
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *lower_dentry = NULL;
	const char *name;
	struct path lower_path;
	struct qstr this;

	/* must initialize dentry operations */
	d_set_d_op(dentry, &u2fs_dops);

	if (IS_ROOT(dentry))
		goto out;

	name = dentry->d_name.name;

	/* now start the actual lookup procedure */
	lower_dir_dentry = lower_parent_path->dentry;
	lower_dir_mnt = lower_parent_path->mnt;

	/* Use vfs_path_lookup to check if the dentry exists or not */
	//if(lower_dir_dentry =! NULL)
	if(idx != NEGATIVE_DENTRY){
		err = vfs_path_lookup(lower_dir_dentry, lower_dir_mnt, name, 0,
					      &lower_path);
	}

	/* no error: handle positive dentries */
	if (!err) {
		if(idx == LEFT){
			u2fs_set_lower_path(dentry, &lower_path, LEFT);
			if(dentry->d_inode != NULL){
				u2fs_set_lower_inode(dentry->d_inode, 
				lower_path.dentry->d_inode, LEFT);
			}
			else{
				err = u2fs_interpose(dentry, dentry->d_sb, &lower_path, LEFT);
				if (err) /* path_put underlying path on error */
					u2fs_put_reset_lower_path(dentry, LEFT);
				goto out;
			}
		}
		else if(idx == RIGHT){
		u2fs_set_lower_path(dentry, &lower_path, RIGHT);
			if(dentry->d_inode != NULL){
                                u2fs_set_lower_inode(dentry->d_inode,
                                lower_path.dentry->d_inode, RIGHT);
               		}
			else{
                       		err = u2fs_interpose(dentry, dentry->d_sb, &lower_path, RIGHT);
                        	if (err) /* path_put underlying path on error */
                               		u2fs_put_reset_lower_path(dentry, RIGHT);
                        	goto out;
			}
		}
	}

	/*
	 * We don't consider ENOENT an error, and we want to return a
	 * negative dentry.
	 */
	if (err && err != -ENOENT)
		goto out;
	
	/* instatiate a new negative dentry */
	if(idx == NEGATIVE_DENTRY){
		this.name = name;
		this.len = strlen(name);
		this.hash = full_name_hash(this.name, this.len);
		lower_dentry = d_lookup(lower_dir_dentry, &this);
		if (lower_dentry)
			goto setup_lower;

		lower_dentry = d_alloc(lower_dir_dentry, &this);
		if (!lower_dentry) {
			err = -ENOMEM;
			goto out;
		}
		d_add(lower_dentry, NULL); /* instantiate and hash */
	
setup_lower:
		lower_path.dentry = lower_dentry;
		lower_path.mnt = mntget(lower_dir_mnt);
		if(mode == NEG_LEFT)
	    		u2fs_set_lower_path(dentry, &lower_path, LEFT);
		else if(mode == NEG_RIGHT)
			u2fs_set_lower_path(dentry, &lower_path, RIGHT);
	}

	/*
	 * If the intent is to create a file, then don't return an error, so
	 * the VFS will continue the process of making this negative dentry
	 * into a positive one.
	 */
	if(idx == NEGATIVE_DENTRY){
		if (flags & (LOOKUP_CREATE|LOOKUP_RENAME_TARGET))
			err = 0;
	}

out:
	return ERR_PTR(err);
}

struct dentry *u2fs_lookup(struct inode *dir, struct dentry *dentry,
			     struct nameidata *nd)
{
/*Performs basic lookup for LB and RB as well as creates negative dentry for whiteouts */
	struct dentry *ret = NULL,*ret_left = NULL, *ret_right = NULL, *parent = NULL;
	struct path lower_parent_path,temp_right;
	struct path lower_parent_path0, lower_parent_path1; 
	int err = 0;
	struct dentry *wh_lower_dentry = NULL;
	unsigned const char *name = NULL;
	name = dentry->d_name.name;

	BUG_ON(!nd);
	parent = dget_parent(dentry);

	u2fs_get_lower_path(parent, &lower_parent_path, LEFT);
	u2fs_get_lower_path(parent, &temp_right, RIGHT);

	
	err = new_dentry_private_data(dentry);
        if (err) {
        	ret_left = ERR_PTR(err);
                goto out;
        }

	/* allocate dentry private data.  We free it in ->d_release */

	//whiteout check start
	u2fs_get_lower_path(parent, &temp_right, LEFT);
	if(temp_right.dentry != NULL){
		wh_lower_dentry = lookup_whiteout(name, temp_right.dentry); 
                if (IS_ERR(wh_lower_dentry)){
                          err = PTR_ERR(wh_lower_dentry);
                          goto out;
                }
		if(wh_lower_dentry->d_inode){
			dput(wh_lower_dentry);
			goto create_neg;
		}
		else
			goto check_left;
	}

check_left:
	if(lower_parent_path.dentry != NULL){
		ret_left = __u2fs_lookup(dentry, nd->flags, &lower_parent_path, LEFT, NORMAL);
		if (IS_ERR(ret_left)){
			goto check_right;
		}
		if (ret_left)
			dentry = ret_left;
		if (dentry->d_inode)
			fsstack_copy_attr_times(dentry->d_inode,
					u2fs_lower_inode_left(dentry->d_inode));
	/* update parent directory's atime */
		fsstack_copy_attr_atime(parent->d_inode,
					u2fs_lower_inode_left(parent->d_inode));
	}

check_right:

	u2fs_get_lower_path(parent, &lower_parent_path, RIGHT);

	if(lower_parent_path.dentry != NULL){	
		ret_right = __u2fs_lookup(dentry, nd->flags, &lower_parent_path, RIGHT, NORMAL);
       		if (IS_ERR(ret_right)){
       	        	goto create_neg;
		}
        	if (ret_right)
               		dentry = ret_right;
        	if (dentry->d_inode && IS_ERR(ret_left)){
                	fsstack_copy_attr_times(dentry->d_inode,
                                        u2fs_lower_inode_right(dentry->d_inode));
        /* update parent directory's atime */
        		fsstack_copy_attr_atime(parent->d_inode,
                	                u2fs_lower_inode_right(parent->d_inode));
		}
		goto out;
	}


create_neg:
	u2fs_get_lower_path(parent, &lower_parent_path0, LEFT);
	u2fs_get_lower_path(parent, &lower_parent_path1, RIGHT);
	if(lower_parent_path0.dentry != NULL){
		ret_right = __u2fs_lookup(dentry, nd->flags, &lower_parent_path0, NEGATIVE_DENTRY, NEG_LEFT);
                	if (IS_ERR(ret_right))
				goto out;
	}
	else if(lower_parent_path1.dentry != NULL){
		ret_right = __u2fs_lookup(dentry, nd->flags, &lower_parent_path1, NEGATIVE_DENTRY, NEG_RIGHT);
                        if (IS_ERR(ret_right))
                                goto out;
	}
	
          
out:
	u2fs_put_lower_path(parent, &lower_parent_path);
	dput(parent);
	
	if(IS_ERR(ret_right))
		ret = ret_left;
	else
		ret = ret_right;

	return ret;
}
