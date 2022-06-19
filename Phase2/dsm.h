#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <err.h>
#include <poll.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <errno.h>

/* fin des includes */

#define TOP_ADDR    (0x40000000)
#define PAGE_NUMBER (10)
#define PAGE_SIZE   (sysconf(_SC_PAGE_SIZE))
#define BASE_ADDR   (TOP_ADDR - (PAGE_NUMBER * PAGE_SIZE))
#define MAX_STR  (1024)
typedef char maxstr_t[MAX_STR];


typedef char maxstr_t[MAX_STR];

/* definition du type des infos */
/* de connexion des processus dsm */
struct dsm_proc_conn  {
   int      rank;
   maxstr_t machine;
   int      port_num;
   int      fd; 
   int      fd_for_exit; /* special */  
};

typedef struct dsm_proc_conn dsm_proc_conn_t; 

typedef enum
{
   NO_ACCESS, 
   READ_ACCESS,
   WRITE_ACCESS, 
   UNKNOWN_ACCESS 
} dsm_page_access_t;

typedef enum
{   
   INVALID,
   READ_ONLY,
   WRITE,
   NO_CHANGE  
} dsm_page_state_t;

typedef int dsm_page_owner_t;

struct dsm_proc {   
  pid_t pid;
  dsm_proc_conn_t connect_info;
};
typedef struct dsm_proc dsm_proc_t;


typedef struct
{ 
   dsm_page_state_t status;
   dsm_page_owner_t owner;
} dsm_page_info_t;

dsm_page_info_t table_page[PAGE_NUMBER];

pthread_t comm_daemon;
extern int DSM_NODE_ID;
extern int DSM_NODE_NUM;


/**************************************************************/
/******************* FIN DE PARTIE NON MODIFIABLE *************/
/**************************************************************/

/* definition du type des infos */
/* d'identification des processus dsm */


char *dsm_init( int argc, char *argv[]);
void  dsm_finalize( void );

#define MAX_STR  (1024)
typedef char maxstr_t[MAX_STR];

typedef enum
{
  DSM_NO_TYPE = -1,
  DSM_REQ,
  DSM_PAGE,
  DSM_NREQ,
  DSM_FINALIZE 
} dsm_req_type_t;

typedef struct
{
  dsm_req_type_t type;
  int source;
  int page_num;
  int new_owner;
} dsm_req_t;
