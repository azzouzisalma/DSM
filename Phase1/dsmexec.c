#include "common_impl.h"


/* variables globales */

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t *proc_array = NULL; 

/* le nombre de processus effectivement crees */
volatile int num_procs_creat = 0;

void usage(void)
{
  fprintf(stdout,"Usage : dsmexec machine_file executable arg1 arg2 ...\n");
  fflush(stdout);
  exit(EXIT_FAILURE);
}

sig_atomic_t nb_procs_alive=0;
void sigchld_handler(int sig)
{
   nb_procs_alive--;
   wait(NULL);
}

/*******************************************************/
/*********** ATTENTION : BIEN LIRE LA STRUCTURE DU *****/
/*********** MAIN AFIN DE NE PAS AVOIR A REFAIRE *******/
/*********** PLUS TARD LE MEME TRAVAIL DEUX FOIS *******/
/*******************************************************/

int main(int argc, char *argv[])
{
   if (argc < 3){
      usage();
   }else{
      pid_t pid;
      int num_procs = 0;

      /* Mise en place d'un traitant pour recuperer les fils zombies*/      
      /* XXX.sa_handler = sigchld_handler; */
      
      
      struct sigaction signal;
      memset(&signal, 0,sizeof(signal));
      signal.sa_handler= sigchld_handler;
      int error=0;
      if(-1==(error= sigaction(SIGCHLD, &signal, NULL)))
         perror("sigaction");
 
      /* Masquage du signal SIGPIPE (pour les writes quand il n'y a pas d'interlocuteur)*/
      sigset_t mask;
      sigemptyset(&mask);
      sigaddset(&mask, SIGPIPE);
      sigprocmask(SIG_BLOCK,&mask,NULL);

      /* lecture du fichier de machines */
      /* 1- on recupere le nombre de processus a lancer */
      /* 2- on recupere les noms des machines : le nom de */
      /* la machine est un des elements d'identification */

      FILE *machinefile=fopen(argv[1],"r");
      maxstr_t machine;

      while(fgets(machine,MAX_STR,machinefile)){
         if(strlen(machine)>1)
            num_procs++;
      }
      rewind(machinefile);

      proc_array = (dsm_proc_t *)malloc(num_procs*sizeof(dsm_proc_t));

      // Création d'un tableau 3D de taille num_procs*2*2
      int ***fds = (int ***)malloc(num_procs*sizeof(int **));
      for(int i=0;i<num_procs;i++){
         fds[i]=(int **)malloc(2*sizeof(int *));
      }
      for(int i=0;i<num_procs;i++){
         for(int j=0;j<2;j++){
            fds[i][j]=(int *)malloc(2*sizeof(int));
         }
      }
      for(int i=0;i<num_procs;i++){
         pipe(fds[i][0]); // Pipe pour stdout
         pipe(fds[i][1]); // Pipe pour stderr
      }
      // Avec fds[i][j], où i est le n° du proc
      // fds[i][0] est le pipe stdout du processus n°i, et fds[i][1] est le pipe pour stderr

      int j=0;
      while(fgets(machine,MAX_STR,machinefile)){
         if(strlen(machine)>2){
            if(machine[strlen(machine)-1]=='\n')
               machine[strlen(machine)-1]='\0';
            dsm_proc_conn_t *tmp = malloc(sizeof(dsm_proc_conn_t));
            strcpy(tmp->machine,machine);
            tmp->port_num=-1;
            proc_array[j].connect_info=*tmp;
            j++;
         }
      }
      fclose(machinefile);

      /* creation de la socket d'ecoute */
      int listen_fd=0;
	   if (-1 == (listen_fd = socket(AF_INET, SOCK_STREAM, 0))) {
		   perror("Socket");
		   exit(EXIT_FAILURE);
	   }
      /* + ecoute effective */ 
      struct addrinfo *res, *tmp;
   	struct addrinfo indices;
	   memset(&indices, 0, sizeof(struct addrinfo)); // fill the pointer with zeros
	   indices.ai_family = AF_INET;
	   indices.ai_socktype = SOCK_STREAM;
	   indices.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	   

	   int err = 0;
	   if (0 != (err = getaddrinfo(NULL, "0", &indices, &res))) { // return a structure addrinfo with information about internet adress
		   errx(1, "%s", gai_strerror(err));
	   }
      tmp=res;

      while(tmp!=NULL){
         if(tmp->ai_addr->sa_family==AF_INET){
            if (-1 == bind(listen_fd, tmp->ai_addr, tmp->ai_addrlen)) {
			      perror("Binding");
		      }
            break;
         }
         tmp=tmp->ai_next;
      }

		if (-1 == listen(listen_fd, num_procs)) {
			perror("Listen");
   	}
      u_short listen_port=-1;
      socklen_t len=sizeof(struct sockaddr);
      struct sockaddr *addr = malloc(sizeof(struct sockaddr));
      memset(addr,0,len);
      getsockname(listen_fd,addr,&len);
      struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
      listen_port=ntohs(addr_in->sin_port);

      /* creation des fils */
      for(int i = 0; i < num_procs ; i++) {
         pid = fork();
         if(pid == 0){ /* fils */
            /* redirection stdout */	 
            close(STDOUT_FILENO);
            dup(fds[i][0][1]);
         
            // /* redirection stderr */	      	      
            close(STDERR_FILENO);
            dup(fds[i][1][1]);
         
            /* Creation du tableau d'arguments pour le ssh */  
            char **newargv = (char **)malloc((argc+5)*sizeof(char *));
            for(int k=0;k<argc+4;k++){
               newargv[k]=(char *)malloc(MAX_STR*sizeof(char));
            }
            strcpy(newargv[0],"ssh");
            strcpy(newargv[1], proc_array[i].connect_info.machine);
            strcpy(newargv[2], "dsmwrap");

            for(int k=3; k<argc+1 ; k++){
               strcpy(newargv[k],argv[k-1]);
            }
            gethostname(newargv[argc+1],MAX_STR);
            sprintf(newargv[argc+2],"%hu",listen_port);
            sprintf(newargv[argc+3],"%d",num_procs);
            newargv[argc+4]=NULL;
            
            /* jump to new prog : */
            execvp("ssh", newargv);
         } else  if(pid > 0) { /* pere */	
            /* fermeture des extremites des tubes non utiles */
            close(fds[i][0][1]); // Fermer l'écriture dans le tube stdout
            close(fds[i][1][1]); // Fermer l'écriture dans le tube stderr

            num_procs_creat++;
            nb_procs_alive++;
            proc_array[i].pid=pid;
            proc_array[i].connect_info.rank=i; 
         }      
      }
      struct pollfd *pollfds= (struct pollfd *)malloc((num_procs_creat+1)*sizeof(struct pollfd));
   	pollfds[0].fd = listen_fd;
	   pollfds[0].events = POLLIN;
	   pollfds[0].revents = 0;
	   // Init remaining slot to default values
	   for (int i = 1; i < num_procs_creat+1; i++) {
		   pollfds[i].fd = -1;
		   pollfds[i].events = 0;
		   pollfds[i].revents = 0;
	   }

      int received_infos=0;
      while(received_infos!=num_procs_creat){
		   if (-1 == poll(pollfds, num_procs_creat+1, -1)) {
			   perror("Poll");
		   }
         for(int i=0;i<num_procs_creat+1;i++){
            if (pollfds[i].fd == listen_fd && pollfds[i].revents & POLLIN) {
               /* on accepte les connexions des processus dsm */
               struct sockaddr client_addr;
               socklen_t size = sizeof(client_addr);
               int client_fd;
               if (-1 == (client_fd = accept(listen_fd, &client_addr, &size))) {
                  perror("Accept");
               }

               for(int j=1;j<num_procs_creat+1;j++){
                  if(pollfds[j].fd==-1){
                     pollfds[j].fd=client_fd;
                     pollfds[j].events=POLLIN;
                     break;
                  }
               }
               dsm_proc_t infos;
               memset(&infos,0,sizeof(dsm_proc_t));
               while(-1==read_from_socket(client_fd,(void*)&infos,sizeof(dsm_proc_t))&&errno!=EPIPE);
               /* On recupere le pid du processus distant  (optionnel)*/
               /* On recupere le numero de port de la socket */
               /* d'ecoute des processus distants */
                     /* cf code de dsmwrap.c */ 
               for(int i=0;i<num_procs_creat;i++){
                  if(!strcmp(proc_array[i].connect_info.machine,infos.connect_info.machine)&&proc_array[i].connect_info.port_num==-1){
                     proc_array[i].connect_info.port_num=infos.connect_info.port_num;
                     proc_array[i].connect_info.fd=client_fd;
                     break;
                  }
               }
               received_infos++;
               pollfds[i].revents=0;
            }
         }
      }


      for(int i=0;i<num_procs_creat;i++){
         printf("Processus n°%d, nom:%s, port:%d\n",i,proc_array[i].connect_info.machine,proc_array[i].connect_info.port_num);
      }

      /***********************************************************/ 
      /********** ATTENTION : LE PROTOCOLE D'ECHANGE *************/
      /********** DECRIT CI-DESSOUS NE DOIT PAS ETRE *************/
      /********** MODIFIE, NI DEPLACE DANS LE CODE   *************/
      /***********************************************************/

      /* 1- envoi du nombre de processus aux processus dsm*/
      /* On envoie cette information sous la forme d'un ENTIER */
      /* (IE PAS UNE CHAINE DE CARACTERES */

      for(int i=1;i<num_procs_creat+1;i++){
         if(pollfds[i].fd!=-1){
            while(-1==write_in_socket(pollfds[i].fd,&num_procs,sizeof(int))&&errno!=EPIPE);
         }
      }
      
      /* 2- envoi des rangs aux processus dsm */
      /* chaque processus distant ne reçoit QUE SON numéro de rang */
      /* On envoie cette information sous la forme d'un ENTIER */
      /* (IE PAS UNE CHAINE DE CARACTERES */

      for(int i=0;i<num_procs_creat;i++){
         while(-1==write_in_socket(proc_array[i].connect_info.fd,&proc_array[i].connect_info.rank,sizeof(int))&&errno!=EPIPE);
      }

      /* 3- envoi des infos de connexion aux processus */
      /* Chaque processus distant doit recevoir un nombre de */
      /* structures de type dsm_proc_conn_t égal au nombre TOTAL de */
      /* processus distants, ce qui signifie qu'un processus */
      /* distant recevra ses propres infos de connexion */
      /* (qu'il n'utilisera pas, nous sommes bien d'accords). */

      for(int i=0;i<num_procs_creat;i++){
         for(int j=0;j<num_procs_creat;j++){
            while(-1==write_in_socket(proc_array[i].connect_info.fd,&proc_array[j].connect_info,sizeof(dsm_proc_conn_t))&&errno!=EPIPE);
         }
      }


      /***********************************************************/
      /********** FIN DU PROTOCOLE D'ECHANGE DES DONNEES *********/
      /********** ENTRE DSMEXEC ET LES PROCESSUS DISTANTS ********/
      /***********************************************************/

      /* gestion des E/S : on recupere les caracteres */
      /* sur les tubes de redirection de stdout/stderr */     
      /* while(1)
         {
            je recupere les infos sur les tubes de redirection
            jusqu'à ce qu'ils soient inactifs (ie fermes par les
            processus dsm ecrivains de l'autre cote ...)
         
         };
      */

      struct pollfd *tubes=(struct pollfd *)malloc(2*num_procs_creat*sizeof(struct pollfd));
      for(int i=0;i<2*num_procs_creat;i+=2){
         tubes[i].fd=fds[i/2][0][0];
         tubes[i+1].fd=fds[i/2][1][0];
         tubes[i].events=POLLIN;
         tubes[i].revents=0;
         tubes[i+1].events=POLLIN;
         tubes[i+1].revents=0;
      }
      while(nb_procs_alive){
         int max_read = 1024;
         char *data_ptr = (char *)malloc(max_read);
         int n_active = 0;
         if (-1 == (n_active = poll(tubes, 2*num_procs_creat, -1))) {
            perror("Poll");
         }
         for(int i=0;i<2*num_procs_creat;i+=2){
            int r=0;
            if(tubes[i].revents&POLLIN){
               while(read(tubes[i].fd,(void *)(data_ptr+r++),1)>0);
               printf("[Proc %i: %s : stdout] %s\n",i/2,proc_array[i/2].connect_info.machine,data_ptr);
               tubes[i].revents=0;
            }
            if(tubes[i+1].revents&POLLIN){
               while(read(tubes[i+1].fd,(void *)(data_ptr+r++),1)>0);
               printf("[Proc %i: %s : stderr] %s\n",i/2,proc_array[i/2].connect_info.machine,data_ptr);
               tubes[i+1].revents=0;
            }
         }
      }

      /* on attend les processus fils */

      /* on ferme les descripteurs proprement */
      for(int i=0; i<num_procs_creat;i++){
         for(int k=0;k<2;k++){
            for(int j=0;j<2;j++){
               close(fds[i][k][j]);
            }
         }
      }
      /* on ferme la socket d'ecoute */
      close(listen_fd);
   }   
   exit(EXIT_SUCCESS);  
}

