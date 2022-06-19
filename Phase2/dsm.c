#include "dsm.h"

int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID;  /* rang (= numero) du processus */ 
int *Nbr_finalize;
int nbr_finalize;
int tube_finalize[2];
dsm_proc_conn_t *machines;
int *conn_sockets;
sem_t*sem ;
int read_from_socket(int fd, void *buf, int size) {
	int ret = 0;
	int offset = 0;
	while (offset != size) {
		ret = read(fd, buf + offset, size - offset);
		if (-1 == ret) {
			return -1;
		}
		offset += ret;
	}
	return offset;
}

int write_in_socket(int fd, void *buf, int size) {
	int ret = 0, offset = 0;
	while (offset != size) {
		if (-1 == (ret = write(fd, buf + offset, size - offset))) {
			return -1;
		}
		offset += ret;
	}
	return offset;
}

/* indique l'adresse de debut de la page de numero numpage */

static char *num2address( int numpage )
{ 
   char *pointer = (char *)(BASE_ADDR+(numpage*(PAGE_SIZE)));
   
   if( pointer >= (char *)TOP_ADDR ){
      fprintf(stderr,"[%i] Invalid address !\n", DSM_NODE_ID);
      return NULL;
   }
   else return pointer;
}

/* cette fonction permet de recuperer un numero de page */
/* a partir  d'une adresse  quelconque */
static int address2num( char *addr )
{
  return (((intptr_t)(addr - BASE_ADDR))/(PAGE_SIZE));
}

/* cette fonction permet de recuperer l'adresse d'une page */
/* a partir d'une adresse quelconque (dans la page)        */
static char *address2pgaddr( char *addr )
{
  return  (char *)(((intptr_t) addr) & ~(PAGE_SIZE-1)); 
}

/* fonctions pouvant etre utiles */
static void dsm_change_info( int numpage, dsm_page_state_t state, dsm_page_owner_t owner)
{
   if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {	
	if (state != NO_CHANGE )
	table_page[numpage].status = state;
      if (owner >= 0 )
	table_page[numpage].owner = owner;
      return;
   }
   else {
	fprintf(stderr,"[%i] Invalid page number !\n", DSM_NODE_ID);
      return;
   }
}

static dsm_page_owner_t get_owner( int numpage)
{
   return table_page[numpage].owner;
}

static dsm_page_state_t get_status( int numpage)
{
   return table_page[numpage].status;
}

/* Allocation d'une nouvelle page */
static void dsm_alloc_page( int numpage )
{
   char *page_addr = num2address( numpage );
   mmap(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   return ;
}

/* Changement de la protection d'une page */
static void dsm_protect_page( int numpage , int prot)
{
   char *page_addr = num2address( numpage );
   mprotect(page_addr, PAGE_SIZE, prot);
   return;
}

static void dsm_free_page( int numpage )
{
   char *page_addr = num2address( numpage );
   munmap(page_addr, PAGE_SIZE);
   return;
}
/* un thread écoutant les communications*/
static void *dsm_comm_daemon( void *arg)
{  
   struct pollfd *pollfds= (struct pollfd *)malloc((DSM_NODE_NUM)*sizeof(struct pollfd));
   int k=0;

   for (int i = 0; i < DSM_NODE_NUM; i++) {
      if(i!=DSM_NODE_ID){
         pollfds[k].fd = conn_sockets[i];
         pollfds[k].events = POLLIN;
         pollfds[k].revents = 0;
         k+=1;
      }
	}
   pollfds[DSM_NODE_NUM-1].fd=tube_finalize[0];
   pollfds[DSM_NODE_NUM-1].events = POLLIN;
   pollfds[DSM_NODE_NUM-1].revents = 0;

   //read_from_socket(pollfds[1].fd,(void*)req,sizeof(dsm_req_t));
   
   while(nbr_finalize)
     {
        	if (-1 == poll(pollfds, DSM_NODE_NUM, -1)) {
			   perror("Poll");
		   }

         for(int i=0;i<DSM_NODE_NUM-1;i++){
            if(pollfds[i].revents & POLLIN){
               dsm_req_type_t type;
               dsm_req_t *req = (dsm_req_t *)malloc(sizeof(dsm_req_t));
               memset(req,0,sizeof(dsm_req_t));
               read_from_socket(pollfds[i].fd,(void*)req,sizeof(dsm_req_t));
               printf("[%i] Je lis a partir de la socket du processus %i\n",DSM_NODE_ID,req->source);
               type=req->type;
               printf("[%i] Le type de la requete est %i\n",DSM_NODE_ID,type);
               switch(type){
                  case DSM_REQ:
                        {                         
                           int source = req->source ;
                           int page = req->page_num ;
                           int fd_source = conn_sockets[source];
                           Nbr_finalize[source]= 0;
                           dsm_req_t *req_response = (dsm_req_t *)malloc(sizeof(dsm_req_t));
                           dsm_req_t *req_response_2 = (dsm_req_t *)malloc(sizeof(dsm_req_t));
                           memset(req_response,0,sizeof(dsm_req_t));
                           memset(req_response_2,0,sizeof(dsm_req_t));

                           req_response->type= DSM_NREQ;
                           req_response->source= DSM_NODE_ID;
                           req_response->page_num= page;
                           req_response->new_owner= source;

                           req_response_2->type= DSM_PAGE;
                           req_response_2->source= DSM_NODE_ID;
                           req_response_2->page_num= page;
                           req_response_2->new_owner= source;
                           char *addr_page= num2address(page);
                           dsm_page_owner_t new_owner= source;
                           dsm_page_state_t new_state= READ_ONLY;
                           char buffer[PAGE_SIZE];
                           strncpy(buffer,addr_page,PAGE_SIZE);
                           write_in_socket(fd_source,(void*)req_response_2,sizeof(dsm_req_t));
                           write_in_socket(fd_source,buffer,PAGE_SIZE);
                           dsm_free_page(page);
                           dsm_change_info(page,new_state,new_owner);
                           for(int j=0;j<DSM_NODE_NUM-1;j++){
                              if(pollfds[j].fd!=fd_source){
                                 write_in_socket(pollfds[j].fd,(void*)req_response,sizeof(dsm_req_t));
                              }
                           }
                           printf("[%i] La page %i a ete envoye a %i\n",DSM_NODE_ID,page,source);
                           break;
                        }
                  case DSM_PAGE:
                        {
                           int source = req->source ;
                           int page = req->page_num ;
                           Nbr_finalize[source]= 0;
                           dsm_page_owner_t new_owner= req->new_owner;
                           dsm_page_state_t new_state= READ_ONLY;
                           char buffer[PAGE_SIZE];
                           memset(buffer,0,PAGE_SIZE);
                           dsm_alloc_page(page);
                           dsm_change_info(page,new_state,new_owner);
                           read_from_socket(pollfds[i].fd,buffer,PAGE_SIZE);
                           char*addr_page= num2address(page);
                           strncpy(addr_page,buffer,PAGE_SIZE);
                           int value = *((int *)(addr_page));
                           printf("[%i] J'ai recu la page %i\n",DSM_NODE_ID,page);
                           sem_post(sem);
                           break;
                        }

                  case DSM_NREQ:
                        {
                           int source = req->source ;
                           int page = req->page_num ;
                           //Nbr_finalize[source]= 0;
                           dsm_page_owner_t new_owner= req->new_owner;
                           dsm_page_state_t new_state= READ_ONLY;
                           dsm_change_info(page,new_state,new_owner);                          
                           printf("[%i] Le nouveau proprietaire de %i est %i\n",DSM_NODE_ID,page,req->new_owner);
                           break;
                        }
                  case DSM_FINALIZE:
                        {
                           int source= req->source;
                           Nbr_finalize[source]= 1; 
                           nbr_finalize--;
                           printf("[%i] Le processus %i a atteint le dsm_finalize\n",DSM_NODE_ID,source); 
                           break;
                        }

               }
               pollfds[i].revents=0;
            }
         }
         
	/* a modifier */

     }
   return NULL;
}

static int dsm_send(int dest,void *buf,size_t size)
{
   /* a completer */
   write_in_socket(dest,buf,size);
  return 0;
}

static int dsm_recv(int from,void *buf,size_t size)
{
   /* a completer */
   read_from_socket(from,buf,size);
  return 0;
}

static void dsm_handler( void *page_addr)
{  
   /* A modifier */
   dsm_page_owner_t owner= -1;
   dsm_page_owner_t new_owner= DSM_NODE_ID;
   dsm_page_state_t new_state= READ_ONLY;
   dsm_req_t *req = (dsm_req_t *)malloc(sizeof(dsm_req_t));
   int numpage= -1;
   int fd_owner=-1;
   numpage= address2num(page_addr);
   owner= get_owner(numpage);
   req->type= DSM_REQ;
   req->source= DSM_NODE_ID;
   req->page_num= numpage;
   /* Send  */
   fd_owner= conn_sockets[owner];
   write_in_socket(fd_owner,(void*)req,sizeof(dsm_req_t));
   printf("[%i] J'ecris sur la socket du proc %i pour demander la page %i\n",DSM_NODE_ID,owner,numpage);  
   sem_wait(sem);
}

/* traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context)
{
   /* A completer */
   /* adresse qui a provoque une erreur */
   void  *addr = info->si_addr;   
  /* Si ceci ne fonctionne pas, utiliser a la place :*/
  /*
   #ifdef __x86_64__
   void *addr = (void *)(context->uc_mcontext.gregs[REG_CR2]);
   #elif __i386__
   void *addr = (void *)(context->uc_mcontext.cr2);
   #else
   void  addr = info->si_addr;
   #endif
   */
   /*
   pour plus tard (question ++):
   dsm_access_t access  = (((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & 2) ? WRITE_ACCESS : READ_ACCESS;   
  */   
   /* adresse de la page dont fait partie l'adresse qui a provoque la faute */
   void  *page_addr  = (void *)(((unsigned long) addr) & ~(PAGE_SIZE-1));

   if ((addr >= (void *)BASE_ADDR) && (addr < (void *)TOP_ADDR))
     {
	dsm_handler(page_addr);
     }
   else
     {
	/* SIGSEGV normal : ne rien faire*/
     }
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char *argv[])
{   
   struct sigaction act;
   int index; 
   
   pipe(tube_finalize);

   sem = (sem_t*)malloc(sizeof(sem_t));
   /* Récupération de la valeur des variables d'environnement */
   /* DSMEXEC_FD et MASTER_FD                                 */
   int dsmexec_fd=atoi(getenv("DSMEXEC_FD"));
   int master_fd=atoi(getenv("MASTER_FD"));
   /* reception du nombre de processus dsm envoye */
   /* par le lanceur de programmes (DSM_NODE_NUM) */
   read_from_socket(dsmexec_fd,&DSM_NODE_NUM,sizeof(int));

   /* reception de mon numero de processus dsm envoye */
   /* par le lanceur de programmes (DSM_NODE_ID)      */
   read_from_socket(dsmexec_fd,&DSM_NODE_ID,sizeof(int));

   /* reception des informations de connexion des autres */
   /* processus envoyees par le lanceur :                */
   /* nom de machine, numero de port, etc.               */
   machines = (dsm_proc_conn_t *)malloc(DSM_NODE_NUM*sizeof(dsm_proc_conn_t));
   nbr_finalize= DSM_NODE_NUM-1;
   Nbr_finalize=(int*)malloc(DSM_NODE_NUM*sizeof(int));
   memset(Nbr_finalize,0,DSM_NODE_NUM*sizeof(int)); 

   for(int i=0;i<DSM_NODE_NUM;i++){
      read_from_socket(dsmexec_fd,&machines[i],sizeof(dsm_proc_conn_t));
   }

   /* initialisation des connexions              */ 
   /* avec les autres processus : connect/accept */
   conn_sockets = (int *)malloc(DSM_NODE_NUM*sizeof(int));
   memset(conn_sockets,-1,DSM_NODE_NUM*sizeof(int));

   if(DSM_NODE_ID!=DSM_NODE_NUM-1){
      if (-1 == listen(master_fd, DSM_NODE_NUM-DSM_NODE_ID-1)){
         perror("Listen");
      }
      for(int i=0;i<DSM_NODE_NUM-DSM_NODE_ID-1;i++){
         struct sockaddr client_addr;
         socklen_t size = sizeof(client_addr);
         int client_fd;
         if (-1 == (client_fd = accept(master_fd, &client_addr, &size))){
            perror("Accept");
         }
         int index=-1;
         read_from_socket(client_fd,&index,sizeof(int));
         conn_sockets[index]=client_fd;
      }
   }

   for(int i=0;i<DSM_NODE_NUM;i++){
      if(machines[i].rank!=DSM_NODE_ID && machines[i].rank<DSM_NODE_ID){
         if (-1 == (conn_sockets[machines[i].rank] = socket(AF_INET, SOCK_STREAM, 0))) {
            perror("Socket");
            exit(EXIT_FAILURE);
         }
         struct addrinfo hints, *res, *tmp;
         memset(&hints, 0, sizeof(struct addrinfo));
         hints.ai_family = AF_INET;
         hints.ai_socktype = SOCK_STREAM;
         hints.ai_flags = AI_NUMERICSERV;
         int error;
         char port[64];
         memset(port,0,64);
         sprintf(port,"%d",machines[i].port_num);
         error = getaddrinfo(machines[i].machine, port, &hints, &res);
         if (error) {
            errx(1, "%s", gai_strerror(error));
            exit(EXIT_FAILURE);
         }
         
         tmp=res;
         while(tmp!=NULL){
            if(tmp->ai_addr->sa_family==AF_INET){
               while(-1==connect(conn_sockets[machines[i].rank],tmp->ai_addr,tmp->ai_addrlen)&&errno==ECONNREFUSED);
               break;
            }
            tmp=tmp->ai_next;
         }
         write_in_socket(conn_sockets[machines[i].rank],&DSM_NODE_ID,sizeof(int));
      }
   }

   /* Initialisation d'un semaphore anonyme */
   sem_init(sem,0,0); //Non partagé et vide

   /* Allocation des pages en tourniquet */
   for(index = 0; index < PAGE_NUMBER; index ++){	
     if ((index % DSM_NODE_NUM) == DSM_NODE_ID){
       dsm_alloc_page(index);	 
     }
      dsm_change_info( index, WRITE, index % DSM_NODE_NUM);
       
   }
   
   /* mise en place du traitant de SIGSEGV */
   act.sa_flags = SA_SIGINFO; 
   act.sa_sigaction = segv_handler;
   sigaction(SIGSEGV, &act, NULL);
   
   /* creation du thread de communication           */
   /* ce thread va attendre et traiter les requetes */
   /* des autres processus                          */
   
   pthread_create(&comm_daemon, NULL, dsm_comm_daemon, NULL);
   
   /* Adresse de début de la zone de mémoire partagée */
   return ((char *)BASE_ADDR);
}

void dsm_finalize( void )
{
   /* fermer proprement les connexions avec les autres processus */

   dsm_req_t *req_finalize= (dsm_req_t*)malloc(sizeof(dsm_req_t));
   memset(req_finalize,0,sizeof(dsm_req_t));
   req_finalize->source= DSM_NODE_ID;
   req_finalize->type= DSM_FINALIZE;
   nbr_finalize= DSM_NODE_NUM-1;
   
   //while(nbr_finalize!= 0){
      for(int j=0;j<DSM_NODE_NUM;j++){
         if(j!=DSM_NODE_ID){
            write_in_socket(conn_sockets[j],(void*)req_finalize,sizeof(dsm_req_t));
            printf("[%i] J'ai envoye un DSM_finalize \n",DSM_NODE_ID);
         }
      }
      /* terminer correctement le thread de communication */
      /* pour le moment, on peut faire :                  */
      for(int i=0;i<DSM_NODE_NUM;i++){
         if(Nbr_finalize[i]==1 && i!=DSM_NODE_ID){
            printf("[%i] Le proc %i a termine \n",DSM_NODE_ID,i);
            nbr_finalize-=1 ;
         }
      }
   
   int fini=0;
   if(nbr_finalize==0){
      write(tube_finalize[1],(void *)&fini,sizeof(int));
   }
   free(machines);
   free(conn_sockets);
   free(Nbr_finalize);
   for(int i=0;i<DSM_NODE_NUM;i++){
      if(i!=DSM_NODE_ID){
         close(conn_sockets[i]);
      }
   pthread_join(comm_daemon,NULL);
   }
}

