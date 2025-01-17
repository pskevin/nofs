/*
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  nofs.log, in the directory from which you run nofs.
*/
#include "config.h"
#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void nofs_localpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, NOFS_DATA->cache_dir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
				    // break here

    log_msg("    nofs_localpath:  cache_dir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    NOFS_DATA->cache_dir, path, fpath);
}

//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void nofs_remotepath(char rpath[PATH_MAX], const char *path)
{
    // in fast mode the server is running at the root directory, so we dont need to prepend it
    if (NOFS_DATA->transport) {
        strcpy(rpath, path+1);
    } else {
        sprintf(rpath, "%s%s", NOFS_DATA->root_dir, path);
    }

    log_msg("    nofs_remotepath: hostname = \"%s\",  root_dir = \"%s\", path = \"%s\", rpath = \"%s\"\n",
	    NOFS_DATA->hostname, NOFS_DATA->root_dir, path, rpath);
}

static void nofs_get_file(char get_cmd[PATH_MAX], const char* source, const char* dest) {
    if (NOFS_DATA->transport) {
        sprintf(get_cmd, "wget -q %s/%s -O %s", NOFS_DATA->hostname, source, dest);
    } else {
        sprintf(get_cmd, "scp -p %s:%s %s", NOFS_DATA->hostname, source, dest);
    }

    log_msg("    get: %s\n", get_cmd);
}

static void nofs_put_file(char put_cmd[PATH_MAX], const char* source, const char* dest) {
    if (NOFS_DATA->transport) {
        sprintf(put_cmd, "curl --silent -F \"file=%s;filename=%s\" %s", source, dest, NOFS_DATA->hostname);
    } else {
        sprintf(put_cmd, "scp -p %s %s:%s", source,  NOFS_DATA->hostname, dest);
    }
    log_msg("    put: %s\n", put_cmd);
}

int force_local_file_update(char lpath[PATH_MAX], const char* path) {
    int retstat;
    char rpath[PATH_MAX];
    nofs_localpath(lpath, path);
    nofs_remotepath(rpath, path);

    char get_cmd[PATH_MAX];
    nofs_get_file(get_cmd, rpath, lpath);
    retstat = system(get_cmd);

    return retstat;
}

int ensure_local_file(char lpath[PATH_MAX], const char* path) {
    int retstat;
    nofs_localpath(lpath, path);

    // First, check if we have a local copy of the file
    retstat = log_syscall("access", access(lpath, F_OK), 0);
    // If not, copy it from remote
    if (retstat != 0) {
        retstat = force_local_file_update(lpath, path);
    }

    return retstat;
}

int persist_local_file_to_remote(const char* lpath, const char* path) {
    char rpath[PATH_MAX];
    nofs_remotepath(rpath, path);
    char put_cmd[PATH_MAX];
    nofs_put_file(put_cmd, lpath, rpath);

    return system(put_cmd);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int nofs_getattr(const char *path, struct stat *statbuf)
{
    int retstat;

    char lpath[PATH_MAX];
    
    log_msg("\nnofs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);

    retstat = ensure_local_file(lpath, path);
    
    retstat = log_syscall("lstat", lstat(lpath, statbuf), 0);
    
    log_stat(statbuf);
    
    return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to nofs_readlink()
// nofs_readlink() code by Bernardo F Costa (thanks!)
int nofs_readlink(const char *path, char *link, size_t size)
{
    int retstat;
    char fpath[PATH_MAX];
    
    log_msg("\nnofs_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    // nofs_fullpath(fpath, path);

    retstat = log_syscall("readlink", readlink(fpath, link, size - 1), 0);
    if (retstat >= 0) {
	link[retstat] = '\0';
	retstat = 0;
	log_msg("    link=\"%s\"\n", link);
    }
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int nofs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat;
    char lpath[PATH_MAX];
    
    log_msg("\nnofs_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    
    int check_remote_existence = ensure_local_file(lpath, path);
    if (check_remote_existence == 0) {
        // This means the file already exists remote, so we should fail
        log_msg("    file already exists on remote. Terminating.\n");
        return -1;
    }

    // On Linux this could just be 'mknod(path, mode, dev)' but this
    // tries to be be more portable by honoring the quote in the Linux
    // mknod man page stating the only portable use of mknod() is to
    // make a fifo, but saying it should never actually be used for
    // that.
    if (S_ISREG(mode)) {
	    retstat = log_syscall("open", open(lpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
	if (retstat >= 0)
	    retstat = log_syscall("close", close(retstat), 0);
    } else
	if (S_ISFIFO(mode))
	    retstat = log_syscall("mkfifo", mkfifo(lpath, mode), 0);
	else
	    retstat = log_syscall("mknod", mknod(lpath, mode, dev), 0);

    return retstat;
}

/** Create a directory */
int nofs_mkdir(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nnofs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
    // nofs_fullpath(fpath, path);

    return log_syscall("mkdir", mkdir(fpath, mode), 0);
}

/** Remove a file */
int nofs_unlink(const char *path)
{
    char fpath[PATH_MAX];
    
    log_msg("nofs_unlink(path=\"%s\")\n",
	    path);
    // nofs_fullpath(fpath, path);

    return log_syscall("unlink", unlink(fpath), 0);
}

/** Remove a directory */
int nofs_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    
    log_msg("nofs_rmdir(path=\"%s\")\n",
	    path);
    // nofs_fullpath(fpath, path);

    return log_syscall("rmdir", rmdir(fpath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int nofs_symlink(const char *path, const char *link)
{
    char flink[PATH_MAX];
    
    log_msg("\nnofs_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    // nofs_fullpath(flink, link);

    return log_syscall("symlink", symlink(path, flink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int nofs_rename(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    log_msg("\nnofs_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    // nofs_fullpath(fpath, path);
    // nofs_fullpath(fnewpath, newpath);

    return log_syscall("rename", rename(fpath, fnewpath), 0);
}
#include <string.h>

/** Create a hard link to a file */
int nofs_link(const char *path, const char *newpath)
{
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    log_msg("\nnofs_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    // nofs_fullpath(fpath, path);
    // nofs_fullpath(fnewpath, newpath);

    return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int nofs_chmod(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nnofs_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    // nofs_fullpath(fpath, path);

    return log_syscall("chmod", chmod(fpath, mode), 0);
}

/** Change the owner and group of a file */
int nofs_chown(const char *path, uid_t uid, gid_t gid)
  
{
    char lpath[PATH_MAX];
    
    log_msg("\nnofs_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    nofs_localpath(lpath, path);

    return log_syscall("chown", chown(lpath, uid, gid), 0);
}

/** Change the size of a file */
int nofs_truncate(const char *path, off_t newsize)
{
    char lpath[PATH_MAX];
    int retstat;

    log_msg("\nnofs_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    retstat = ensure_local_file(lpath, path);
    if (retstat != 0) {
        return retstat;
    }

    retstat = log_syscall("truncate", truncate(lpath, newsize), 0);
    return retstat;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int nofs_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    
    log_msg("\nnofs_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    // nofs_fullpath(fpath, path);

    return log_syscall("utime", utime(fpath, ubuf), 0);
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int nofs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char lpath[PATH_MAX];

    log_msg("\nnofs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);
    int retval = ensure_local_file(lpath, path);
    
    // if we cant find a remote file, and we're not creating a new one, something is wrong
    if (retval != 0 && !(fi->flags & O_CREAT)) {
        return retval;
    }

    // Force local file copy if we are opening the file for writing
    if (fi->flags & (O_RDWR | O_WRONLY)) {
        force_local_file_update(lpath, path);
    }
    
    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.
    fd = log_syscall("open", open(lpath, fi->flags), 0);
    if (fd < 0)
    retstat = log_error("open");
    
    fi->fh = fd;

    log_fi(fi);
    
    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int nofs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nnofs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return log_syscall("pread", pread(fi->fh, buf, size, offset), 0);
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int nofs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nnofs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi
	    );
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int nofs_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nnofs_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    // nofs_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);
    
    log_statvfs(statv);
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this will flush to the remote server if the file was opened for writing.
int nofs_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nnofs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int nofs_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nnofs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);

    char lpath[PATH_MAX];

    nofs_localpath(lpath, path);

    if (fi->flags & (O_RDWR | O_WRONLY)) {
        // then we need to copy it back.
        int persist = persist_local_file_to_remote(lpath, path);
        if (persist != 0) {
            return persist;
        }
    }

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int nofs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\nnofs_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    int retval;
    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
	retval = log_syscall("fdatasync", fdatasync(fi->fh), 0);
    else
#endif	
	retval = log_syscall("fsync", fsync(fi->fh), 0);

    if (retval != 0) {
        return retval;
    }

    char lpath[PATH_MAX];

    nofs_localpath(lpath, path);

    if (fi->flags & (O_RDWR | O_WRONLY)) {
        // then we need to copy it back.
        int persist = persist_local_file_to_remote(lpath, path);
        if (persist != 0) {
            return persist;
        }
    }

    return retval;
}

#ifdef HAVE_SYS_XATTR_H
/** Note that my implementations of the various xattr functions use
    the 'l-' versions of the functions (eg nofs_setxattr() calls
    lsetxattr() not setxattr(), etc).  This is because it appears any
    symbolic links are resolved before the actual call takes place, so
    I only need to use the system-provided calls that don't follow
    them */

/** Set extended attributes */
int nofs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    char lpath[PATH_MAX];
    
    log_msg("\nnofs_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	    path, name, value, size, flags);
    int retstat = ensure_local_file(lpath, path);
    if (retstat != 0) {
        return retstat;
    }

    return log_syscall("lsetxattr", lsetxattr(lpath, name, value, size, flags), 0);
}

/** Get extended attributes */
int nofs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char lpath[PATH_MAX];
    
    log_msg("\nnofs_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);

    retstat = ensure_local_file(lpath, path);

    retstat = log_syscall("lgetxattr", lgetxattr(lpath, name, value, size), 0);
    if (retstat >= 0)
	log_msg("    value = \"%s\"\n", value);
    
    return retstat;
}

/** List extended attributes */
int nofs_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char lpath[PATH_MAX];
    char *ptr;
    
    log_msg("\nnofs_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
	    path, list, size
	    );

    retstat = ensure_local_file(lpath, path);
    if (retstat != 0) {
        return retstat;
    }

    retstat = log_syscall("llistxattr", llistxattr(lpath, list, size), 0);
    if (retstat >= 0) {
	log_msg("    returned attributes (length %d):\n", retstat);
	if (list != NULL)
	    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
		log_msg("    \"%s\"\n", ptr);
	else
	    log_msg("    (null)\n");
    }
    
    return retstat;
}

/** Remove extended attributes */
int nofs_removexattr(const char *path, const char *name)
{
    char lpath[PATH_MAX];
    
    log_msg("\nnofs_removexattr(path=\"%s\", name=\"%s\")\n",
	    path, name);

    int retstat = ensure_local_file(lpath, path);
    if (retstat != 0) {
        return retstat;
    }

    return log_syscall("lremovexattr", lremovexattr(lpath, name), 0);
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int nofs_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nnofs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    // nofs_fullpath(fpath, path);

    // since opendir returns a pointer, takes some custom handling of
    // return status.
    dp = opendir(fpath);
    log_msg("    opendir returned 0x%p\n", dp);
    if (dp == NULL)
	retstat = log_error("nofs_opendir opendir");
    
    fi->fh = (intptr_t) dp;
    
    log_fi(fi);
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int nofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    struct dirent *de;
    
    log_msg("\nnofs_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    log_msg("    readdir returned 0x%p\n", de);
    if (de == 0) {
	retstat = log_error("nofs_readdir readdir");
	return retstat;
    }

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
	log_msg("calling filler with name %s\n", de->d_name);
	if (filler(buf, de->d_name, NULL, 0) != 0) {
	    log_msg("    ERROR nofs_readdir filler:  buffer full");
	    return -ENOMEM;
	}
    } while ((de = readdir(dp)) != NULL);
    
    log_fi(fi);
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int nofs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nnofs_releasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    log_fi(fi);
    
    closedir((DIR *) (uintptr_t) fi->fh);
    
    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int nofs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nnofs_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *nofs_init(struct fuse_conn_info *conn)
{
    log_msg("\nnofs_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    return NOFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void nofs_destroy(void *userdata)
{
    log_msg("\nnofs_destroy(userdata=0x%08x)\n", userdata);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int nofs_access(const char *path, int mask)
{
    int retstat = 0;
    char lpath[PATH_MAX];
   
    log_msg("\nnofs_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
    nofs_localpath(lpath, path);
    
    retstat = access(lpath, mask);
    
    if (retstat < 0)
	retstat = log_error("nofs_access access");
    
    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
// Not implemented.  I had a version that used creat() to create and
// open the file, which it turned out opened the file write-only.

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int nofs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nnofs_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
	    path, offset, fi);
    log_fi(fi);
    
    retstat = ftruncate(fi->fh, offset);
    if (retstat < 0)
	retstat = log_error("nofs_ftruncate ftruncate");
    
    return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int nofs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nnofs_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
	return nofs_getattr(path, statbuf);
    
    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	retstat = log_error("nofs_fgetattr fstat");
    
    log_stat(statbuf);
    
    return retstat;
}

struct fuse_operations nofs_oper = {
  .getattr = nofs_getattr,
  .readlink = nofs_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = nofs_mknod,
  .mkdir = nofs_mkdir,
  .unlink = nofs_unlink,
  .rmdir = nofs_rmdir,
  .symlink = nofs_symlink,
  .rename = nofs_rename,
  .link = nofs_link,
  .chmod = nofs_chmod,
  .chown = nofs_chown,
  .truncate = nofs_truncate,
  .utime = nofs_utime,
  .open = nofs_open,
  .read = nofs_read,
  .write = nofs_write,
  /** Just a placeholder, don't set */ // huh???
  .statfs = nofs_statfs,
  .flush = nofs_flush,
  .release = nofs_release,
  .fsync = nofs_fsync,
  
#ifdef HAVE_SYS_XATTR_H
  .setxattr = nofs_setxattr,
  .getxattr = nofs_getxattr,
  .listxattr = nofs_listxattr,
  .removexattr = nofs_removexattr,
#endif
  
  .opendir = nofs_opendir,
  .readdir = nofs_readdir,
  .releasedir = nofs_releasedir,
  .fsyncdir = nofs_fsyncdir,
  .init = nofs_init,
  .destroy = nofs_destroy,
  .access = nofs_access,
  .ftruncate = nofs_ftruncate,
  .fgetattr = nofs_fgetattr
};

void nofs_usage()
{
    fprintf(stderr, "usage:  nofs [FUSE and mount options] fastTransfer hostName rootDir mountPoint cacheDir\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct nofs_state *nofs_data;

    // nofs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running nofs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0)) {
    	fprintf(stderr, "Running NOFS as root opens unnacceptable security holes\n");
    	return 1;
    }

    // See which version of fuse we're running
    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 4) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-') || (argv[argc-3][0] == '-') || (argv[argc-4][0] == '-')  || (argv[argc-5][0] == '-'))
	nofs_usage();

    nofs_data = malloc(sizeof(struct nofs_state));
    if (nofs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the root_dir and hostname out of the argument list and save it in my
    // internal data
    nofs_data->root_dir = argv[argc-3];
    nofs_data->transport = atoi(argv[argc-5]); // 0 = SCP, 1 = HTTP (via wget/curl)
    nofs_data->hostname = argv[argc-4];
    nofs_data->cache_dir = argv[argc-1];
    argv[argc-5] = argv[argc-2];
    argv[argc-4] = NULL;
    argv[argc-3] = NULL;
    argv[argc-2] = NULL;
    argv[argc-1] = NULL;
    argc-= 4;
    
    nofs_data->logfile = log_open();
    
    printf("Created remote file system at host: %s, remote root dir: %s, mount point: %s, cache dir: %s\n", nofs_data->hostname, nofs_data->root_dir, argv[argc-1], nofs_data->cache_dir);
    fflush(0);
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &nofs_oper, nofs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    return fuse_stat;
}
