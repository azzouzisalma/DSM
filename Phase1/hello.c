#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[]){

    const char* path1= getenv("DSMEXEC_FD");
    const char* path2= getenv("MASTER_FD");
    printf("la variable d'env dans hello: DSMEXEC est %s et MASTER est %s\n", path1,path2);
    return 0;
    sleep(10);
} 
