
#include "wrapfs.h"
//#include "utils.h"

#define U2FS_WHPFX ".wh."
#define U2FS_WHLEN 4
#define U2FS_DIR_OPAQUE_NAME "__dir_opaque"
#define U2FS_DIR_OPAQUE U2FS_WHPFX U2FS_DIR_OPAQUE_NAME


//External functions used in this module
void release_lower_nd(struct nameidata *nd, int err);
int init_lower_nd(struct nameidata *nd, unsigned int flags);
static inline struct dentry *lookup_lck_len(const char *name,
                                            struct dentry *base, int len);


bool is_validname(const char *name)
{
//checks if whiteout name is valid
	if (!strncmp(name, U2FS_WHPFX, U2FS_WHLEN)){
        	return false;
	}
        if (!strncmp(name, U2FS_DIR_OPAQUE_NAME,
        	sizeof(U2FS_DIR_OPAQUE_NAME) - 1)){
        	return false;
	}
	return true;
}

void dput_lower_dentry(struct dentry *dentry){

	if(U2FS_D(dentry)->lower_path[0].dentry != NULL){
		dput(U2FS_D(dentry)->lower_path[0].dentry);
		mntput(U2FS_D(dentry)->lower_path[0].mnt);
	}
	if(U2FS_D(dentry)->lower_path[1].dentry != NULL){
		dput(U2FS_D(dentry)->lower_path[1].dentry);
                mntput(U2FS_D(dentry)->lower_path[1].mnt);
        }
	kfree(U2FS_D(dentry)->lower_path[0].dentry);
	kfree(U2FS_D(dentry)->lower_path[1].dentry);
	U2FS_D(dentry)->lower_path[0].dentry = NULL;
	U2FS_D(dentry)->lower_path[1].dentry = NULL;
}

char *alloc_whname(const char *name, int len)
{
//Allocaates a whiteout name starting with .wh.
	char *buf;
        buf = kmalloc(len + U2FS_WHLEN + 1, GFP_KERNEL);
        if (unlikely(!buf))
                return ERR_PTR(-ENOMEM);
   
        strcpy(buf, U2FS_WHPFX);
        strlcat(buf, name, len + U2FS_WHLEN + 1);
   
        return buf;
}



struct dentry *lock_parent_wh(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
        mutex_lock_nested(&dir->d_inode->i_mutex, U2FS_DMUTEX_WHITEOUT);
        return dir;
}

static inline struct dentry *lookup_lck_len(const char *name,
                                             struct dentry *base, int len)
{
//Looks for a whiteout on the left directory at same level as current right directory
	struct dentry *d;
        struct nameidata lower_nd;
        int err;
        err = init_lower_nd(&lower_nd, LOOKUP_OPEN);
        if (unlikely(err < 0)) {
		d = ERR_PTR(err);
                goto out;
        }
        mutex_lock(&base->d_inode->i_mutex);
        d = lookup_one_len(name, base, len);
        release_lower_nd(&lower_nd, err);
        mutex_unlock(&base->d_inode->i_mutex);
out:
        return d;
}

void u2fs_postwh_setmnt(struct dentry *dentry)
{
//Mounts the file/directory created
	struct dentry *parent, *hasone;
        if (U2FS_D(dentry)->lower_path[0].mnt)
		return;
        hasone = dentry->d_parent;
        /* this loop should stop at root dentry */
        while (!U2FS_D(hasone)->lower_path[0].mnt)
                hasone = hasone->d_parent;
        parent = dentry;
        while (!U2FS_D(parent)->lower_path[0].mnt) {
                U2FS_D(parent)->lower_path[0].mnt = mntget(U2FS_D(hasone)->lower_path[0].mnt);
               /* unionfs_set_lower_mnt_idx(parent, bindex,
                                          unionfs_mntget(hasone, bindex));*/
                parent = parent->d_parent;
       }
}


int create_whiteout(struct dentry *dentry){

//Creates a whiteout in LB of a file to be deleted from RB.
	struct dentry *lower_dir_dentry = NULL;
	struct dentry *lower_dentry = NULL;
	struct dentry *lower_wh_dentry = NULL;
	struct nameidata nd;
	char *name = NULL;
	int err = -EINVAL;

	name = alloc_whname(dentry->d_name.name, dentry->d_name.len);

	if (unlikely(IS_ERR(name))) {
		err = PTR_ERR(name);
		goto out;
	}

	lower_dentry = U2FS_D(dentry)->lower_path[0].dentry;
	lower_wh_dentry = lookup_lck_len(name, lower_dentry->d_parent,
			    dentry->d_name.len + U2FS_WHLEN);

	if (IS_ERR(lower_wh_dentry)){
		goto out;
	}

	if (lower_wh_dentry->d_inode){
		dput(lower_wh_dentry);
		err = 0;
		goto out;
	}	

	err = init_lower_nd(&nd, LOOKUP_CREATE);

	if (unlikely(err < 0))
		goto out;

	lower_dir_dentry = lock_parent_wh(lower_wh_dentry);

	if (!err){
		err = vfs_create(lower_dir_dentry->d_inode,
					  lower_wh_dentry,
				  current_umask() & S_IRUGO,
					   &nd);
	}
	unlock_dir(lower_dir_dentry);
	dput(lower_wh_dentry);
	release_lower_nd(&nd, err);    
  
	u2fs_postwh_setmnt(dentry);   	
	
out:
        kfree(name);
        return err;
}

struct dentry *lookup_whiteout(const char *name, struct dentry *lower_parent)
{
//Finds if a whiteout of a file in RB exists in LB at same directory level
	char *whname = NULL;
        int err = 0, namelen;
        struct dentry *wh_dentry = NULL;

        namelen = strlen(name);
        whname = alloc_whname(name, namelen);
        if (unlikely(IS_ERR(whname))) {
		err = PTR_ERR(whname);
                goto out;
        }
  
          /* check if whiteout exists in this branch: lookup .wh.foo */
	wh_dentry = lookup_lck_len(whname, lower_parent, strlen(whname));

        if (IS_ERR(wh_dentry)) {
                err = PTR_ERR(wh_dentry);
                goto out;
        }
         /* check if negative dentry (ENOENT) */
        if (!wh_dentry->d_inode)
		goto out;
  
         /* whiteout found: check if valid type */
        if (!S_ISREG(wh_dentry->d_inode->i_mode)) {
                printk(KERN_ERR "u2fs: invalid whiteout %s entry type %d\n",
                       whname, wh_dentry->d_inode->i_mode);
                dput(wh_dentry);
                err = -EIO;
                goto out;
        }
 
out:
	kfree(whname);
        if (err)
		wh_dentry = ERR_PTR(err);
	return wh_dentry;
}
