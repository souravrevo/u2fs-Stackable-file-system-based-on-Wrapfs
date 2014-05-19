

#include "wrapfs.h"

void release_lower_nd(struct nameidata *nd, int err);
int init_lower_nd(struct nameidata *nd, unsigned int flags);

int __u2fs_rename(struct inode *old_dir, struct dentry *old_dentry,
                               struct dentry *old_parent,
                               struct inode *new_dir, struct dentry *new_dentry,
                               struct dentry *new_parent, int idx)
{
	int err = 0;
        struct dentry *lower_old_dentry;
        struct dentry *lower_new_dentry;
        struct dentry *lower_old_dir_dentry;
        struct dentry *lower_new_dir_dentry;
        struct dentry *trap;

        lower_new_dentry = U2FS_D(new_dentry)->lower_path[LEFT].dentry;
	lower_old_dentry = U2FS_D(old_dentry)->lower_path[idx].dentry;
	
	dget(lower_old_dentry);
        dget(lower_new_dentry);
        lower_old_dir_dentry = dget_parent(lower_old_dentry);
        lower_new_dir_dentry = dget_parent(lower_new_dentry);
  
	trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
         
        if (trap == lower_old_dentry) {
		err = -EINVAL;
                goto out_err_unlock;
        }
          
        if (trap == lower_new_dentry) {
                err = -ENOTEMPTY;
                goto out_err_unlock;
        }
	 
        err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
                         lower_new_dir_dentry->d_inode, lower_new_dentry);
	
out_err_unlock:
        if (!err) {
                fsstack_copy_attr_times(old_dir, lower_old_dir_dentry->d_inode);
                fsstack_copy_attr_times(new_dir, lower_new_dir_dentry->d_inode);
        }
        unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
  
        dput(lower_old_dir_dentry);
        dput(lower_new_dir_dentry);
        dput(lower_old_dentry);
        dput(lower_new_dentry);         
        return err;
}
  
