#include "common_impl.h"

int main(int argc, char **argv)
{   
   /* processus intermediaire pour "nettoyer" */
   /* la liste des arguments qu'on va passer */
   /* a la commande a executer finalement  */
  int dsmexec_fd=0;
  int er;
  char **newargv = (char **)malloc((argc-3)*sizeof(char *)); // le premier et le deuxieme arg sont @IP et port
  for(int k=0;k<argc-4;k++){
    newargv[k]=(char *)malloc(MAX_STR*sizeof(char));
    strcpy(newargv[k],argv[k+1]);
  }
  newargv[argc-4]=NULL;

  char **arge = (char **)malloc(2*sizeof(char *)); // le premier et le deuxieme arg sont @IP et port
  for(int k=0;k<2;k++){
    arge[k]=(char *)malloc(MAX_STR*sizeof(char));
  }

  char *hostname = argv[argc-3];
  char *port= argv[argc-2];
  int num_procs = atoi(argv[argc-1]);
 
   /* creation d'une socket pour se connecter au */
   /* au lanceur et envoyer/recevoir les infos */
   /* necessaires pour la phase dsm_init */   

  dsmexec_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct addrinfo hints, *res, *tmp;
	memset(&hints, 0, sizeof(struct addrinfo)); // fill the pointer with zeros
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;
	int error;
	error = getaddrinfo(hostname, port, &hints, &res); // return a structure addrinfo with information about internet adress ,port=2
	if (error) {
		errx(1, "%s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}
    
  // Connection
	tmp=res;
  while(tmp!=NULL){
    if(tmp->ai_addr->sa_family==AF_INET){
      if(-1==connect(dsmexec_fd,tmp->ai_addr,tmp->ai_addrlen)){
        perror("Connect");
        exit(EXIT_FAILURE);
      }
      break;
    }
    tmp=tmp->ai_next;
  }
  /* Creation de la socket d'ecoute pour les */
  /* connexions avec les autres processus dsm */

  int master_fd=0;
  if (-1 == (master_fd = socket(AF_INET, SOCK_STREAM, 0))) {
    perror("Socket");
    exit(EXIT_FAILURE);
  }

  /* + ecoute effective */ 
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

  int err = 0;
  if (0 != (err = getaddrinfo(NULL, "0", &hints, &res))) {
    errx(1, "%s", gai_strerror(err));
  }

  tmp=res;
  while(tmp!=NULL){
    if(tmp->ai_addr->sa_family==AF_INET){
      if (-1 == bind(master_fd, tmp->ai_addr, tmp->ai_addrlen)) {
      perror("Binding");
    }
      break;
    }
    tmp=tmp->ai_next;
  }

  if (-1 == listen(master_fd, num_procs)) {
    perror("Listen");
  }

  dsm_proc_t infos;
  memset(&infos,0,sizeof(dsm_proc_t));

  /* Envoi du nom de machine au lanceur */
  /* Envoi du pid au lanceur (optionnel) */

  dsm_proc_conn_t connect_infos;
  memset(&connect_infos,0,sizeof(dsm_proc_conn_t));
  maxstr_t name;
  memset(name,0,MAX_STR);
  gethostname(name, MAX_STR);
  strcpy(connect_infos.machine,name);
  infos.pid=getpid();
  

  /* Envoi du numero de port au lanceur */
  /* pour qu'il le propage à tous les autres */
  /* processus dsm */

  u_short port_number=-1;
  socklen_t len=sizeof(struct sockaddr);
  struct sockaddr *addr = malloc(sizeof(struct sockaddr));
  getsockname(master_fd,addr,&len);
  struct sockaddr_in *lol = (struct sockaddr_in *)addr;
  port_number=ntohs(lol->sin_port);
  connect_infos.port_num=port_number;
  infos.connect_info=connect_infos;
  write_in_socket(dsmexec_fd,&infos,sizeof(dsm_proc_t));

  /* on execute la bonne commande */
  /* attention au chemin à utiliser ! */

  strcpy(arge[0],"DSMEXEC_FD");
  strcpy(arge[1],"MASTER_FD");
  char str_masterfd[64];
  char str_dsmexecfd[64];
  sprintf(str_masterfd,"%d",master_fd);
  sprintf(str_dsmexecfd,"%d",dsmexec_fd);
  if(-1==(er=setenv(arge[0], str_dsmexecfd,1))){ // FOR OTHERS PROCESSUS
    perror("setenv");
  }
  if(-1==(er=setenv(arge[1], str_masterfd,1))){ // FOR DSMEXEC
    perror("setenv");
  }

  execvp(newargv[0],newargv);

  /************** ATTENTION **************/
  /* vous remarquerez que ce n'est pas   */
  /* ce processus qui récupère son rang, */
  /* ni le nombre de processus           */
  /* ni les informations de connexion    */
  /* (cf protocole dans dsmexec)         */
  /***************************************/

  return 0;
}
