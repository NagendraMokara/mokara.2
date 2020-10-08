#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/types.h>

#include "palindromes.h"

static struct palindromes* setup_shm(){
	const key_t key = ftok(FTOK_PATH, FTOK_KEY);
	if(key == -1){
		perror("ftok");
		return NULL;
	}

	const int memid = shmget(key, sizeof(struct palindromes), 0);
  if(memid == -1){
  	perror("semget");
  	return NULL;
  }

	struct palindromes * addr = (struct palindromes *) shmat(memid, NULL, 0);
	if(addr == NULL){
		perror("shmat");
		return NULL;
	}

	return addr;
}

static int get_xx(const int argc, char * const argv[]){

	if(argc != 2){
		fprintf(stderr, "Usage: palin xx\n");
		return -1;
	}

	int xx = atoi(argv[1]);
	if(xx < 0){
		fprintf(stderr, "Error: xx > 0!\n");
		return -1;
	}

	return xx;
}

static int word_or_palindrome(const char * str){
	int li = 0, ri = strlen(str) - 1;

	while(li < ri){
		const char lc = tolower(str[li++]);	//left char
		const char rc = tolower(str[ri--]);	//right char

		if(lc != rc){
			return 0;	//word
		}
	}

	return 1;	//palindrome
}

int main(const int argc, char * const argv[]){

	struct palindromes *ptr;
	const int xx = get_xx(argc, argv);
	const char *palin_files[2] = {"nopalin.out", "palin.out"};

	if(xx < 0){
		return EXIT_FAILURE;
	}

	ptr = setup_shm();
	if(ptr == NULL){
		return EXIT_FAILURE;
	}

	srand(getpid());


	const int palin = word_or_palindrome(ptr->strings[xx]);	// 0 for word, 1 for palindrom
	const int section = (palin == 0) ? 0 : 1;

	sem_wait(&ptr->sem[SEM_CLOCK]);
	fprintf(stderr, "PID %d WAITING FOR %s at %li.%li\n", getpid(), palin_files[section],
		ptr->shared_clock.tv_sec, ptr->shared_clock.tv_usec);
	sem_post(&ptr->sem[SEM_CLOCK]);

	sem_wait(&ptr->sem[section]);

	/* critical section */

	sem_wait(&ptr->sem[SEM_CLOCK]);
	fprintf(stderr, "PID %d LOCKED %s at %li.%li\n", getpid(), palin_files[section],
			ptr->shared_clock.tv_sec, ptr->shared_clock.tv_usec);
	sem_post(&ptr->sem[SEM_CLOCK]);

	FILE * fp = fopen(palin_files[section], "a");
	if(fp == NULL){
		perror("fopen");
	}else{
		sleep(rand() % 2);

		fprintf(fp, "%s\n", ptr->strings[xx]);
		fclose(fp);
	}

	sem_post(&ptr->sem[section]);

	sem_wait(&ptr->sem[SEM_CLOCK]);
	fprintf(stderr, "PID %d UNLOCKED %s at %li.%li\n", getpid(), palin_files[section],
		ptr->shared_clock.tv_sec, ptr->shared_clock.tv_usec);
	sem_post(&ptr->sem[SEM_CLOCK]);

	shmdt(ptr);

	return EXIT_SUCCESS;
}
