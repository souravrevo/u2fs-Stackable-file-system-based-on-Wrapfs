#include "wrapfs.h"
#include <linux/module.h>

/*
 * There is no need to lock the u2fs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */

static int u2fs_read_super(struct super_block *sb, void *raw_data, int silent)
{
/*Main function to mount the file system. It parses LDIR and RDIR and mounts both in U2FS */

	int err = 0;
	struct super_block *lower_sb[2];
	struct path lower_path[2];
	char *dev_name = (char *) raw_data;
	struct inode *inode;
	//parse variablse start
	char *rdir = kmalloc(sizeof(char *),GFP_KERNEL);
	char *ldir = kmalloc(sizeof(char *),GFP_KERNEL);
        int ldx = 0, rdx = 0, i = 0, cdx = 0;
        char *lptr = NULL, *rptr = NULL, *comma = NULL, *equal = NULL;
	//parse variables end
	
	UDBG;
	
	//parse start

	lptr = strstr(dev_name,"ldir");
        rptr = strstr(dev_name,"rdir");
	comma =  strchr (dev_name, ',');
	equal = strchr(dev_name,'=');

        if(!lptr || !rptr || !comma || (ldx != 0) || !equal || strlen(dev_name) < 12 ){
                err = -EPERM;
		goto out;
        }
        else{
                ldx = lptr - dev_name;
                rdx = rptr - dev_name;
		cdx = comma - dev_name;
		printk("dev_name: %s \n",dev_name);
		printk("cdx %d \n",cdx);
	}

	for(i = 5; i < cdx ; i++){
                ldir[i-5] = dev_name[i];
        }
	ldir[cdx - 5] = '\0';

        for(i = 0; i < strlen(dev_name) - rdx; i++){
                rdir[i] = dev_name[i+rdx+5];
       	}

	//parse end


	if (!dev_name) {
		printk(KERN_ERR
		       "u2fs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(ldir, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path[LEFT]);
	if (err) {
		printk(KERN_ERR	"u2fs: error accessing "
		       "lower directory '%s'\n", dev_name);
		goto out;
	}

	err = kern_path(rdir, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
                        &lower_path[RIGHT]);
        if (err) {
                printk(KERN_ERR "u2fs: error accessing "
                       "lower directory '%s'\n", dev_name);
                goto out;
        }


	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct u2fs_sb_info), GFP_KERNEL);
	if (!U2FS_SB(sb)) {
		printk(KERN_CRIT "u2fs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out_free;
	}

	/* set the lower superblock field of upper superblock */
	lower_sb[LEFT] = lower_path[LEFT].dentry->d_sb;
	atomic_inc(&lower_sb[LEFT]->s_active);
	u2fs_set_lower_super(sb, lower_sb[LEFT], LEFT);
	
	lower_sb[RIGHT] = lower_path[RIGHT].dentry->d_sb;
        atomic_inc(&lower_sb[RIGHT]->s_active);
        u2fs_set_lower_super(sb, lower_sb[RIGHT], RIGHT);


	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb[LEFT]->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &u2fs_sops;

	/* get a new inode and allocate our root dentry */
	inode = u2fs_iget1(sb, lower_path[LEFT].dentry->d_inode, lower_path[RIGHT].dentry->d_inode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_alloc_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &u2fs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	u2fs_set_lower_path(sb->s_root, &lower_path[LEFT], LEFT);
	u2fs_set_lower_path(sb->s_root, &lower_path[RIGHT], RIGHT);
	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_alloc_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		printk(KERN_INFO
		       "u2fs: mounted on top of %s type %s\n",
		       dev_name, lower_sb[LEFT]->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb[LEFT]->s_active);
	kfree(U2FS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path[LEFT]);
	path_put(&lower_path[RIGHT]);

out:
	kfree(ldir);
	kfree(rdir);
	return err;
}

struct dentry *u2fs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	void *lower_path_name = (void *) raw_data;
	UDBG;

	return mount_nodev(fs_type, flags, lower_path_name,
			   u2fs_read_super);
}

static struct file_system_type u2fs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= U2FS_NAME,
	.mount		= u2fs_mount,
	.kill_sb	= generic_shutdown_super,
	.fs_flags	= FS_REVAL_DOT,
};

static int __init init_u2fs_fs(void)
{
	int err;

	UDBG;
	pr_info("Registering u2fs " U2FS_VERSION "\n");

	err = u2fs_init_inode_cache();
	if (err)
		goto out;
	err = u2fs_init_dentry_cache();
	
	err = init_sioq();
        if (unlikely(err))
                 goto out;	

	if (err)
		goto out;
	err = register_filesystem(&u2fs_fs_type);
out:
	if (err) {
		stop_sioq();
		u2fs_destroy_inode_cache();
		u2fs_destroy_dentry_cache();
	}
		
	return err;
}

static void __exit exit_u2fs_fs(void)
{
	UDBG;
	stop_sioq();
	u2fs_destroy_inode_cache();
	u2fs_destroy_dentry_cache();
	unregister_filesystem(&u2fs_fs_type);
	pr_info("Completed u2fs module unload\n");
}

MODULE_AUTHOR("Kumar Sourav, MSCS Student, Stony Brook Universty");
MODULE_DESCRIPTION("u2fs "U2FS_VERSION);
MODULE_LICENSE("GPL");

module_init(init_u2fs_fs);
module_exit(exit_u2fs_fs);
