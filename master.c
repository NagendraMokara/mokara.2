#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/shm.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "palindromes.h"

static int n = 0, nmax = 4;   //processes
static int s = 0, smax = 2;   //running processes
static int e = 0, emax = 0;   //exited processes
static int        tmax = 100; //runtime
static int w = 0, wmax = 0;   //strings (words)

static int memid = -1;
static pid_t pids[20];

static struct palindromes * ptr = NULL;

static int loop_flag = 1;

static void wait_children(){
  pid_t pid;
  int status;

  while((pid = waitpid(-1, &status, WNOHANG)) > 0){

    //clear from pids[]
    int i;
    for(i=0; i < s; i++){
      if(pids[i] == pid){
        pids[i] = 0;
      }
    }

    --s;
    if(++e >= nmax){
      loop_flag = 0;
    }
  }
}

static int term_children(){
  int i;
  for(i=0; i < s; ++i){
    if(pids[i] > 0){
      kill(pids[i], SIGTERM);
    }
  }

  while(s > 0){
    wait_children();
  }

  return 0;
}

static void sig_handler(const int sig){

  switch(sig){
    case SIGINT:
    case SIGALRM:
      loop_flag = 0;
      break;

    case SIGCHLD:
      wait_children();
      break;

    default:
      break;
  }
}

static void clean_shm(){
  int i;
  for(i=0; i < SEMN; i++){
    sem_destroy(&ptr->sem[i]);
  }
  shmctl(memid, IPC_RMID, NULL);
  shmdt(ptr);
}

static int exec_palin(const int word_index){
  char xx[20];

  const pid_t pid = fork();
  switch(pid){
    case -1:
      perror("fork");
      break;

    case 0:
      snprintf(xx, 20, "%i", word_index);

      setpgid(getpid(), getppid());
      execl("palin", "palin", xx, NULL);

      perror("execl");
      exit(EXIT_FAILURE);

      break;

    default:
      pids[s] = pid;
      break;
  }

	return pid;
}


static int setup_words(const char * wordfilename){
  int i = 0;

  FILE * wordfile = fopen(wordfilename, "r");
  if(wordfile == NULL){
    perror("fopen");
    return -1;
  }

  for(i=0; i < LLEN; ++i){
    if(fgets(ptr->strings[i], WLEN, wordfile) == NULL){
      break;
    }
    const int len = strlen(ptr->strings[i]);
    ptr->strings[i][len-1] = '\0';  /* remove the newline */
  }
  fclose(wordfile);

  return i;
}

static int setup_args(const int argc, char * const argv[]){

  int option;
	while((option = getopt(argc, argv, "n:s:t:h")) != -1){
		switch(option){
			case 'h':
        fprintf(stderr, "Usage: master [-n x] [-s x] [-t x] infile.txt\n");
        fprintf(stderr, "-n x Processes to start\n");
        fprintf(stderr, "-s x Processes to rununing\n");
        fprintf(stderr, "-t x Runtime\n");
        fprintf(stderr, "-h Show this message\n");
				return -1;

      case 'n':  nmax	= atoi(optarg); break;
			case 's':  smax	= atoi(optarg); break;
      case 't':  tmax	= atoi(optarg); break;

      default:
				fprintf(stderr, "master: Error: Invalid option\n");
				return -1;
		}
	}

  if( (smax <= 0) || (smax > 20)   ){
    fprintf(stderr, "Error: -s invalid\n");
    return -1;
  }
  emax = nmax;

  if(optind == (argc -1)){
    wmax = setup_words(argv[argc-1]);
  }else{
    fprintf(stderr, "Error: No wordfile specified\n");
    return -1;
  }

  /* If processes are more than the words */
  if(nmax > wmax){
    nmax = wmax;  /* change number of processes to match words count */
  }

  return 0;
}

static struct palindromes * setup_shm(){

	const key_t key = ftok(FTOK_PATH, FTOK_KEY);
	if(key == -1){
		perror("ftok");
		return NULL;
	}

	memid = shmget(key, sizeof(struct palindromes), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
  if(memid == -1){
  	perror("shmget");
  	return NULL;
  }

	struct palindromes * addr = (struct palindromes *) shmat(memid, NULL, 0);
	if(addr == NULL){
		perror("shmat");
		return NULL;
	}

  bzero(addr, sizeof(struct palindromes));

  int i;
  for(i=0; i < SEMN; i++){
    if(sem_init(&addr->sem[i], 1, 1) == -1){
      perror("sem_init");
      return NULL;
    }
  }

	return addr;
}

static int shared_clock_update(){
  struct timeval timestep, temp;

  timestep.tv_sec = 0;
  timestep.tv_usec = 1000;

  sem_wait(&ptr->sem[SEM_CLOCK]);
  timeradd(&ptr->shared_clock, &timestep, &temp);
  ptr->shared_clock = temp;
  sem_post(&ptr->sem[SEM_CLOCK]);

  return 0;
}

int main(const int argc, char * const argv[]){


  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, sig_handler);
  signal(SIGCHLD, sig_handler);
  signal(SIGALRM, sig_handler);


  ptr = setup_shm();
  if(ptr == NULL){
    clean_shm();
    return EXIT_FAILURE;
  }

  if(setup_args(argc, argv) == -1){
    clean_shm();
    return EXIT_FAILURE;
  }

  alarm(tmax);  /* setup alarm after arguments are processes*/
  bzero(pids, sizeof(pid_t)*20);

	while(loop_flag && (n < nmax)){

    //if we can, we start a new process
    if( (w < wmax) && (n < nmax) && (s < smax)  ){
      const pid_t palin_pid = exec_palin(w);
      if(palin_pid > 0){
        printf("%i %d %s at %li.%li\n", palin_pid, w, ptr->strings[w], ptr->shared_clock.tv_sec, ptr->shared_clock.tv_usec);
        ++n; ++s; ++w;  /* increase count of processes started, running and words */
      }
    }
    shared_clock_update();
	}

  term_children();
	clean_shm();
  return EXIT_SUCCESS;
}
