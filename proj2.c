#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/wait.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>


/*
gcc proj2.c -o proj2 -std=gnu99 -Wall -Wextra -Werror -pedantic
$ ./proj2 NE NR TE TR
•NE: počet skřítků. 0<NE<1000
•NR: počet sobů. 0<NR< 20
•TE: Maximální doba v milisekundách, po kterou skřítek pracuje samostatně. 0<=TE<=1000.
•TR: Maximální doba v milisekundách, po které se sob vrací z dovolené domů. 0<=RE<=1000.
•Všechny parametry jsou nezáporná celá čísla.
*/


typedef struct sharedvar {
  int helped_id[3];
  int elf_id_queue[1000];
  int rnd_queue[20];
  int rnd_hitched[20];
  int rnd_count;
  int process_count;
  int christmas;
  int hitched;
  int workshop_closed;

} sharedvar;


void cleanup(int* shm_id,sharedvar* shared_var, sem_t *sem, char* sem_name ){
  sem_unlink(sem_name);
  sem_close(sem);
  shmdt(shared_var); shmctl(*shm_id,IPC_RMID,0);
}


void SpawnRnd(int NR, int TR, sharedvar* shared_var, sem_t *sem){

  pid_t pid_rnd[NR];
  int i;
  int n = NR;

  /* Start children. */
  for (i = 1; i <= n; i++) {
  if ((pid_rnd[i] = fork()) < 0) {
    perror("fork");
    exit(1);
  } else if (pid_rnd[i] == 0) {
    //rndthings
    sem_wait(sem);
    shared_var->process_count+=1;
    printf("%d: RD %d: rstarted\n",shared_var->process_count,i );
    sem_post(sem);

    time_t t;
    srand((unsigned) time(&t));
    usleep(((rand() % (TR - TR/2 + 1)) + TR/2 )*1000);

    sem_wait(sem);
    shared_var->process_count+=1;
    printf("%d: RD %d: return home\n",shared_var->process_count,i );
    sem_post(sem);

    sem_wait(sem);
    for (int a = 0; a < NR; a++) {
      if(shared_var->rnd_queue[a]==0){
        shared_var->rnd_queue[a]=i;
        shared_var->rnd_count++;
        // printf("Rnd %d in rnd_queue[%d] rnd_count=%d\n",i,a,shared_var->rnd_count ); //test print
        break;
      }
    }
    sem_post(sem);


    while(shared_var->rnd_count!=0);

    for (int a = 0; a < NR; a++) {
      if(shared_var->rnd_hitched[a]==i){
        sem_wait(sem);
        shared_var->process_count+=1;
        printf("%d: RD %d: get hitched\n",shared_var->process_count,i );
        shared_var->rnd_count++;
        sem_post(sem);
        exit(0);

      }
    }




    }
  }

  while (n > 0) {
  wait(NULL);
  n--;
  }
  exit(0);

}


void SpawnElf(int NE, int TE, sharedvar* shared_var, sem_t *sem){

  pid_t pid_elf[NE];
  int i;
  int n = NE;

  /* Start children. */
  for (i = 1; i <= n; i++) {
  if ((pid_elf[i] = fork()) < 0) {
    perror("fork");
    exit(1);
  } else if (pid_elf[i] == 0) {
    //elfthings
    int wait_status;

    sem_wait(sem);
    shared_var->process_count+=1;
    printf("%d: Elf %d: started\n",shared_var->process_count,i );
    sem_post(sem);

    while(shared_var->workshop_closed==0){

      time_t t;
      srand((unsigned) time(&t));
      usleep(((rand() % TE)*1000));

      sem_wait(sem);
      shared_var->process_count+=1;
      printf("%d: Elf %d: need help\n",shared_var->process_count,i );
      sem_post(sem);

      sem_wait(sem);
      for (int a = 0; a < NE; a++) {
        if(shared_var->elf_id_queue[a]==0){
          shared_var->elf_id_queue[a]=i;
          wait_status=1;
          // printf("Elf %d is in elf_id_queue[%d] and wait_status=%d\n",i,a,wait_status );
          break;
        }
      }
      sem_post(sem);

      while(wait_status==1 && shared_var->workshop_closed==0){

        for (int a = 0; a < 3; a++) {
          if(shared_var->helped_id[a]==i){
            sem_wait(sem);
            shared_var->helped_id[a]=0;
            shared_var->process_count+=1;
            printf("%d: Elf %d: get help\n",shared_var->process_count,i );
            wait_status=0;
            sem_post(sem);
          }
        }

      }

    }
    sem_wait(sem);
    shared_var->process_count+=1;
    printf("%d: Elf %d: taking holidays\n",shared_var->process_count,i );
    sem_post(sem);


    exit(0);
    }
  }

  while (n > 0) {
  wait(NULL);
  n--;
  }
  exit(0);
}


void SpawnSanta(int NR, int NE, sharedvar* shared_var, sem_t *sem){

  pid_t pid_santa = fork();
  if(pid_santa == 0){
      //santa thing
      int sleep_status;
      int elf;
      int temp;
      int elves_needed;
      if(NE>3){
        elves_needed=4;
      }else{
        elves_needed=3;
      }
      while(shared_var->christmas == 0){
        sem_wait(sem);
        shared_var->process_count+=1;
        printf("%d: Santa: going to sleep\n",shared_var->process_count );
        sem_post(sem);
        sleep_status=1;


        while(sleep_status==1){
          if(shared_var->rnd_count==NR){
            break;
          }

          elf=0;
          for (int a = 0; a < elves_needed; a++) {
            if(shared_var->elf_id_queue[a] != 0){
              elf++;
              if(elf==elves_needed){
                // printf("%d elves are gathered\n",elf );
                sem_wait(sem);
                shared_var->helped_id[0]=shared_var->elf_id_queue[0];
                shared_var->helped_id[1]=shared_var->elf_id_queue[1];
                shared_var->helped_id[2]=shared_var->elf_id_queue[2];
                shared_var->elf_id_queue[0]=0;
                shared_var->elf_id_queue[1]=0;
                shared_var->elf_id_queue[2]=0;
                sem_post(sem);
                sleep_status=0;
              }
            }
          }

        }
        if(elf==elves_needed){
          sem_wait(sem);
          shared_var->process_count+=1;
          printf("%d: Santa: helping elves\n",shared_var->process_count );
          sem_post(sem);

          while(shared_var->helped_id[0] != 0 || shared_var->helped_id[1] != 0 || shared_var->helped_id[2] !=0 );

          if(NE>3){

            // for (int a = 0; a < NE; a++) {
            //   printf("%d ",shared_var->elf_id_queue[a]);
            // }
            // printf("\n");


            sem_wait(sem);
            temp = shared_var->elf_id_queue[0];
            while(shared_var->elf_id_queue[0]==0){
              for (int a = 0; a < NE - 1; a++){
                    shared_var->elf_id_queue[a] = shared_var->elf_id_queue[a + 1];
              }
              shared_var->elf_id_queue[NE-1] = temp;
            }
            sem_post(sem);

            // for (int a = 0; a < NE; a++) {
            //   printf("%d ",shared_var->elf_id_queue[a]);
            // }
            // printf("\n");

          }
        }

        if(shared_var->rnd_count==NR){
          sem_wait(sem);
          shared_var->process_count+=1;
          printf("%d: Santa: closing workshop\n",shared_var->process_count );
          shared_var->workshop_closed=1;
          sem_post(sem);

          for (int a = 0; a < NR; a++) {
            sem_wait(sem);
            shared_var->rnd_hitched[a]=shared_var->rnd_queue[a];
            shared_var->rnd_count--;
            sem_post(sem);
          }
            while(shared_var->hitched==0){
              if(shared_var->rnd_count==NR){
                break;
              }
            }
            sem_wait(sem);
            shared_var->process_count+=1;
            printf("%d: Santa: Christmas started\n",shared_var->process_count );
            sem_post(sem);
            shared_var->christmas=1;
        }
      }

    exit(0);
  }else{
    while(wait(NULL) != -1 || errno != ECHILD);
    exit(0);
  }

}


int main(int argc,char** argv) {

  for (int i = 1; i < argc; i++) {
    if (atoi(argv[i])<=0) {
      printf("Invalid number: '%s'\n",argv[i]);
      return 1;
    }
  }
  int NE = atoi(argv[1]);
  int NR = atoi(argv[2]);
  int TE = atoi(argv[3]);
  int TR = atoi(argv[4]);

  char* sem_name = "xsmutn15.semaphore";

  //shared memory
  sharedvar *shared_var;

  key_t shm_key = 0;
  shm_key++;
  shm_key--;  //unused variable
  shm_key = ftok("/dev/null",1);
  int shm_id = shmget(shm_id, sizeof(sharedvar)+NE*sizeof(int), 0644 | IPC_CREAT);

  if(shm_id<0){
    fprintf(stderr,"Error has occured while creating shm_id.\n");
    exit(1);
  }

  shared_var = shmat(shm_id,NULL,0);

  //semaphore
  sem_t *sem = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 1 );
  if(sem == SEM_FAILED){
    perror("semaphore couldn't be created");
    cleanup(&shm_id, shared_var, sem, sem_name);
    exit(1);
  }

  pid_t process_handler = fork();
  if(process_handler==0){
    //child / parent
    // printf("start\n" );
    process_handler = fork();
    if(process_handler>0){
        SpawnSanta(NR,NE, shared_var, sem);
    }else if(process_handler==0){
      process_handler = fork();
      if(process_handler>0){
        // printf("nothing\n" );
        SpawnElf(NE, TE, shared_var, sem);
      }else if(process_handler==0){
        process_handler=fork();
        if(process_handler>0){
          SpawnRnd(NR, TR, shared_var, sem);
          // printf("nothing\n" );
        }
      }
    }

  }else if(process_handler>0){
    //parent waiting for all children and grandchildren
    while(wait(NULL) != -1 || errno != ECHILD);
    cleanup(&shm_id, shared_var, sem, sem_name);
    // printf("end\n" );
  }

  return 0;
}
