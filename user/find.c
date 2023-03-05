#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

char* get_file_name(char *path) {
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) {
      printf("get_file_name error\n");
      exit(1);
  }
  return p;
}

// find with a abs path
void find(char *path, const char *filename) {
    char *p;
  int fd;
  struct stat st;
  struct dirent de;
  int filename_len = 0;
  int print_path_len = 0;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_DIR:
      // extend
      p = path + strlen(path);
      *p = '/'; // write '\0' to '/'
        p++;
        *p = '\0';

      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, "..")) {
          continue;
        }
        filename_len = strlen(de.name);
        if (de.name[filename_len] != '\0') {
            fprintf(2, "error while de.name ending without zero\n");
            exit(1);
        }
        p = strcpy(p, de.name); // store ptr to before de.name
        p[filename_len] = '\0';

        find(path, filename);
        *p = '\0'; // drop de.name
      }
      break;
    case T_FILE:
      // ONLY FILE IS LEAF NODE
      // print format: filename\n\0
      p = get_file_name(path);
      if (!strcmp(filename, p)) {
          print_path_len = strlen(path);
            p = path + print_path_len;
            *p = '\n';
            // p++;
            // *p='\0';
              write(1, path, print_path_len+1);
              // write(1, "\n", 1);
      }
      break;
  }
  close(fd);
  return;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(2, "error usage find\n");
    exit(0);
  }

  char *path = argv[1];
  char *filename = argv[2];
  find(path, filename);
  exit(0);
  return 0;
}
