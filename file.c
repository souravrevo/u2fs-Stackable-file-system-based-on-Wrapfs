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

static ssize_t u2fs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	lower_file = u2fs_lower_file_left(file);
	if(lower_file == NULL)
		lower_file = u2fs_lower_file_right(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);

	return err;
}

static ssize_t u2fs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;
	lower_file = u2fs_lower_file_left(file);
	if(lower_file == NULL){
		lower_file = u2fs_lower_file_right(file);
		if(lower_file == NULL)
			return err;
	err = copyup_file(file->f_dentry->d_inode, file);
	goto out;
	}
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
		fsstack_copy_attr_times(dentry->d_inode,
					lower_file->f_path.dentry->d_inode);
	}

out:
	return err;
}

static int u2fs_filldir(void *dirent, const char *oname, int namelen,
                        loff_t offset, u64 ino, unsigned int d_type){

	struct u2fs_getdents_callback *buf = dirent;
	char *wh_name = NULL;
        int err = 0, i = 0;
	void *p;
        bool is_whiteout = false, duplicate = false, already_exist = false, whiteout_name = false ; 
        char *name = (char *) oname;
	off_t pos;
	u64 u2fs_ino;
	
	if(buf->idx == LEFT){
		for(i = 0 ; i < buf->count ; i++){
			if(strcmp(buf->names[i],name) == 0){
				already_exist = true;
			}	
		}			
	
		if(!already_exist){
			if(buf->count != 0){
                                p = krealloc(buf->names , (buf->count + 1) * sizeof(char*), GFP_KERNEL);
                                buf->names = p;
                        }
			buf->names[buf->count] = name;
			buf->count++;
		}
	}
	
	if(buf->idx == RIGHT){	
		for(i = 0; i < buf->count ; i++){
			if(strcmp(buf->names[i],name) == 0){
				duplicate = true;	
			}
		}

	}
		
	//Eliminate whiteout start

	 if(buf->idx == RIGHT){
		wh_name = alloc_whname(name, strlen(name));		
                for(i = 0; i < buf->count ; i++){
                        if(strcmp(buf->names[i],wh_name) == 0){
                                is_whiteout = true;
                        }
                }

        }

	//Eliminate whiteout end

	//skip whiteout names start

	if (!is_validname(name)) {
                err = -EPERM;
                whiteout_name = true;
         }

	//skip whiteout names end
	pos = 1;
        u2fs_ino = ino; 
		
	if((!duplicate) && (!is_whiteout)){
        	err = buf->filldir(buf->dirent, name, namelen, pos,
                                 u2fs_ino, d_type);
	}
	return err;

}

static int u2fs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	int err = 0;
	struct file *lower_file[2] = {NULL};
	struct dentry *dentry = file->f_path.dentry;
	struct u2fs_getdents_callback buf;
        //struct u2fs_dir_state *uds;
	lower_file[LEFT] = u2fs_lower_file_left(file);
	lower_file[RIGHT] = u2fs_lower_file_right(file);

	//skip file buf parameters

	buf.filldir_called = 0;
        buf.filldir_error = 0;
        buf.entries_written = 0;
        buf.dirent = dirent;
        buf.filldir = filldir;
        //buf.rdstate = uds;
        buf.sb = file->f_dentry->d_inode->i_sb;
	buf.names = kmalloc(sizeof(char*), GFP_KERNEL);
	buf.count = 0;

	if(lower_file[LEFT] != NULL){
		buf.idx = LEFT;
		err = vfs_readdir(lower_file[LEFT], u2fs_filldir, &buf);
		file->f_pos = lower_file[LEFT]->f_pos;
	}
	if(lower_file[RIGHT] != NULL){
		buf.idx = RIGHT;
		err = vfs_readdir(lower_file[RIGHT], u2fs_filldir, &buf);
		file->f_pos = lower_file[RIGHT]->f_pos;
	}

	if (err >= 0){		/* copy the atime */
		if(lower_file[LEFT] != NULL){
			fsstack_copy_attr_atime(dentry->d_inode,
						lower_file[LEFT]->f_path.dentry->d_inode);
		}

		if(lower_file[RIGHT] != NULL){
			fsstack_copy_attr_atime(dentry->d_inode,
                        	                lower_file[RIGHT]->f_path.dentry->d_inode);
		}
	}
	
	kfree(buf.names);	
	return err;
}

static long u2fs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	lower_file = u2fs_lower_file_left(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

out:
	return err;
}

#ifdef CONFIG_COMPAT
static long u2fs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	lower_file = u2fs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int u2fs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;
	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = u2fs_lower_file_left(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "u2fs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!U2FS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "u2fs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
		err = do_munmap(current->mm, vma->vm_start,
				vma->vm_end - vma->vm_start);
		if (err) {
			printk(KERN_ERR "u2fs: do_munmap failed %d\n", err);
			goto out;
		}
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &u2fs_vm_ops;
	vma->vm_flags |= VM_CAN_NONLINEAR;

	file->f_mapping->a_ops = &u2fs_aops; /* set our aops */
	if (!U2FS_F(file)->lower_vm_ops) /* save for our ->fault */
		U2FS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int u2fs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file[2] = {NULL};
	struct path lower_path[2];

	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}
	file->private_data =
		kzalloc(sizeof(struct u2fs_file_info), GFP_KERNEL);
	if (!U2FS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link u2fs's file struct to lower's */
	u2fs_get_lower_path(file->f_path.dentry, &lower_path[LEFT], LEFT);
	u2fs_get_lower_path(file->f_path.dentry, &lower_path[RIGHT], RIGHT);

	if((U2FS_D(file->f_dentry)->lower_path[LEFT].dentry == NULL) || (U2FS_D(file->f_dentry)->lower_path[LEFT].mnt == NULL) ){
                goto check_right;
	}
	else{
		lower_file[LEFT] = dentry_open(lower_path[LEFT].dentry, lower_path[LEFT].mnt,
					 file->f_flags, current_cred());
		if (IS_ERR(lower_file[LEFT])) {
               		err = PTR_ERR(lower_file[LEFT]);
               	 	lower_file[LEFT] = u2fs_lower_file_left(file);
               		if (lower_file[LEFT]) {
                        	u2fs_set_lower_file(file, NULL, LEFT);
                        	fput(lower_file[LEFT]); /* fput calls dput for lower_dentry */
                	}
		} else{
               	 	u2fs_set_lower_file(file, lower_file[LEFT], LEFT);
        	}
	}
	
check_right:
	if((U2FS_D(file->f_dentry)->lower_path[RIGHT].dentry == NULL) || (U2FS_D(file->f_dentry)->lower_path[RIGHT].mnt == NULL)){
                goto out_err;
        }
	else{
		lower_file[RIGHT] = dentry_open(lower_path[RIGHT].dentry, lower_path[RIGHT].mnt,
                	                 file->f_flags, current_cred());
	
		if (IS_ERR(lower_file[RIGHT])) {
			err = PTR_ERR(lower_file[RIGHT]);
			lower_file[RIGHT] = u2fs_lower_file_right(file);
			if (lower_file[RIGHT]) {
				u2fs_set_lower_file(file, NULL, RIGHT);
				fput(lower_file[RIGHT]); /* fput calls dput for lower_dentry */
			}
		} else {
			u2fs_set_lower_file(file, lower_file[RIGHT], RIGHT);
		}
	}

	if (err)
		kfree(U2FS_F(file));
	else{
		if(!((lower_path[LEFT].dentry == NULL) || (lower_path[LEFT].mnt == NULL))){
			fsstack_copy_attr_all(inode, u2fs_lower_inode_left(inode));
		}
		 if(!((lower_path[RIGHT].dentry == NULL) || (lower_path[RIGHT].mnt == NULL))){
                        fsstack_copy_attr_all(inode, u2fs_lower_inode_right(inode));
                }

	}
out_err:
	return err;
}

static int u2fs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;
	lower_file = u2fs_lower_file_left(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush)
		err = lower_file->f_op->flush(lower_file, id);
	 lower_file = u2fs_lower_file_right(file);
        if (lower_file && lower_file->f_op && lower_file->f_op->flush)
                err = lower_file->f_op->flush(lower_file, id);

	return err;
}

/* release all lower object references & free the file info structure */
static int u2fs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;
	lower_file = u2fs_lower_file_left(file);
	if (lower_file) {
		u2fs_set_lower_file(file, NULL, LEFT);
		fput(lower_file);
	}
	lower_file = u2fs_lower_file_right(file);
        if (lower_file) {
                u2fs_set_lower_file(file, NULL, RIGHT);
                fput(lower_file);
        }
	kfree(U2FS_F(file));
	return 0;
}

static int u2fs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err = 0;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;
	err = generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = u2fs_lower_file_left(file);
	if(lower_file != NULL)
		u2fs_get_lower_path(dentry, &lower_path, LEFT);
	else{
		lower_file = u2fs_lower_file_right(file);
		if(lower_file != NULL)
			u2fs_get_lower_path(dentry, &lower_path, RIGHT);
		else{
			err = -ENOENT;
			goto out;
		}
			
	}
	err = vfs_fsync_range(lower_file, start, end, datasync);
	u2fs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int u2fs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;
	lower_file = u2fs_lower_file_left(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

const struct file_operations u2fs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= u2fs_read,
	.write		= u2fs_write,
	.unlocked_ioctl	= u2fs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= u2fs_compat_ioctl,
#endif
	.mmap		= u2fs_mmap,
	.open		= u2fs_open,
	.flush		= u2fs_flush,
	.release	= u2fs_file_release,
	.fsync		= u2fs_fsync,
	.fasync		= u2fs_fasync,
};

/* trimmed directory options */
const struct file_operations u2fs_dir_fops = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= u2fs_readdir,
	.unlocked_ioctl	= u2fs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= u2fs_compat_ioctl,
#endif
	.open		= u2fs_open,
	.release	= u2fs_file_release,
	.flush		= u2fs_flush,
	.fsync		= u2fs_fsync,
	.fasync		= u2fs_fasync,
};
