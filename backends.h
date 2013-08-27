void be_add(char *name);
void be_freeall();
void *be_stab(char *key);
void be_dump();

// typedef char *(*f_init)(void);
typedef void (f_kill)(void *conf);
typedef char *(f_getuser)(void *conf, const char *username);
typedef int (f_superuser)(void *conf, const char *username);
typedef int (f_aclcheck)(void *conf, const char *username, const char *topic, int acc);
