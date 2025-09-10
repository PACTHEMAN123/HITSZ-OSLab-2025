#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

char *get_name(char *path) {
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  return p;
}


/*
 * usage of find: 
 * find <path> -name <name>
 * find files(name) under the path
 * (some code copy from ls.c)
 */ 
void find(char *path, char *name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    /* open the 'path' dir as a fd */
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    /* get the path dir stat */
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    /* the path must be dir */ 
    if (st.type != T_DIR) {
        fprintf(2, "find: path should be dir\n", path);
        close(fd);
        return;
    }
    /* buffer overflow? */
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
        fprintf(2, "find: path too long\n", path);
        close(fd);
        return;
    }

    /* buf: path/ */
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    /* read the path dir and search */
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        /* skip invalid dentry */
        if(de.inum == 0)
            continue;        
        /* get a dentry, try to stat */
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        // printf("[%s] [%s] %d\n",name, buf, strcmp(name, get_name(buf)));
        /* find the target file/dir (name) */
        if(strcmp(name, get_name(buf)) == 0) 
            printf("%s\n", buf);

        /* recursive search, skip "." and ".." dentry */
        if(st.type == T_DIR && (strcmp(get_name(buf), ".") && strcmp(get_name(buf), "..")))
            find(buf, name);
    }
    
    close(fd);
    return;
}

int main(int argc, char *argv[])
{
    if(argc < 3){
        fprintf(2, "find: missing arguments\n");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}
