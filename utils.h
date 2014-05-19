
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)



void u2fs_postcopyup_setmnt(struct dentry *dentry)
{
/*This funtion mounts the newly created file/dir after copyup */
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
                 parent = parent->d_parent;
  	}
}    


void u2fs_postcopyup_release(struct dentry *dentry)
{
        BUG_ON(S_ISDIR(dentry->d_inode->i_mode));
   	U2FS_D(dentry)->lower_path[1].dentry = NULL;
	U2FS_D(dentry)->lower_path[1].mnt = NULL;
	u2fs_set_lower_inode(dentry->d_inode, NULL, 1);
}

void release_lower_nd(struct nameidata *nd, int err)
{
        if (!nd->intent.open.file)
                return;
        //else if (!err)
                //release_open_intent(nd);
#ifdef ALLOC_LOWER_ND_FILE
        kfree(nd->intent.open.file);
#endif
}

int init_lower_nd(struct nameidata *nd, unsigned int flags)
{
	int err = 0;
        
#ifdef ALLOC_LOWER_ND_FILE
        struct file *file;
#endif
        memset(nd, 0, sizeof(struct nameidata));
	if (!flags)
		return err;

        switch (flags) {
        case LOOKUP_CREATE:
                nd->intent.open.flags |= O_CREAT;

        case LOOKUP_OPEN:
                nd->flags = flags;
                nd->intent.open.flags |= (FMODE_READ | FMODE_WRITE);
#ifdef ALLOC_LOWER_ND_FILE
                file = kzalloc(sizeof(struct file), GFP_KERNEL);
                if (unlikely(!file)) {
                        err = -ENOMEM;
                        break;
                }
                nd->intent.open.file = file;
#endif
                break;
        default:
                pr_debug("u2fs: unknown nameidata flag x%x\n", flags);
                BUG();
                break;
        }

        return err;
}

static inline struct dentry *lookup_lck_len(const char *name,
                                             struct dentry *base, int len)
{
/*This function looks for a file in current directory level*/
	struct dentry *d;
        struct nameidata lower_nd;
        int err;
	UDBG;
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



