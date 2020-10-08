#define FTOK_PATH "/root"
#define FTOK_KEY 3450

#define WLEN 50
#define LLEN 100

enum SEMNAME {SEM_NOFILE=0, SEM_FILE, SEM_CLOCK, SEMN};

struct palindromes {
	sem_t sem[SEMN];
	struct timeval shared_clock;
	char strings[LLEN][WLEN];
};
