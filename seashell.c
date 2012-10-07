#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>

/* 
   seashelll.c
   Author: Michael E. Karpeles
   Desc: Simple Bash Like Shell
 */

// #####################################################################################
// C O N S T A N T S
// #####################################################################################
const int HISTBUFFSIZE = 64;

const int TERMINATED  = 0;
const int FOREGROUND  = 1;
const int STOPPED     = 2;

const int NONE        = 0;
const int DETATCH     = 1; // &
const int SOURCE      = 2; // ;
const int CONNECTOR   = 4; // &&
const int REDIR_IN    = 5; // <
const int REDIR_OUT   = 6; // >
const int REDIR_OUT2  = 7; // >>
const int REDIR_PIPE  = 8; // |

typedef void (*sighandler_t)(int);

char *custom_prompt = NULL;

// #####################################################################################
// D A T A - S T R U C T U R E S
// #####################################################################################

// ===========================
// Structures for organization
// ===========================
struct session
{
  char *prompt_name;
  //char *hist[32];
  //int hindex;
  //struct pidtable *pidtab;
};

/* struct pidtable
{
  pid_t pids[64];
}; */

struct arg
{
  char *str;
  struct arg *next;
};

struct cmd 
{
  int dir; // redirection type
  int opt; // status: source, detatch
  int con;
  struct arg *argn;
  struct cmd *next;
};

// ========================
// Struct Methods & Helpers
// ========================

// ------------------------
// Cmd Methods
// ------------------------

int cmd_count (struct cmd *first)
{
  int cnt = 1;
  struct cmd *iter;
  iter = first;
  while(iter->next != NULL) {
    iter = iter->next;
    cnt++;
  }
  return cnt;
}

struct cmd *nth_cmd(struct cmd *first, int n)
{
  int cnt = 1;
  struct cmd *iter;
  iter = first;
  while(iter->next != NULL) {  
    if (cnt == n) {
      return iter;
    } 
    iter = iter->next;
    cnt++;
  } 
  iter = NULL;
  return iter;
}


// ------------------------
// Arg Methods
// ------------------------

// [1] count the number of args in the linked list
int arg_count (struct arg *first) 
{
  int cnt = 1;
  struct arg *iter;
  iter = first;
  while(iter->next != NULL) {
    iter = iter->next;
    cnt++;
  }
  return cnt;
}

// [2] Returns the nth arg in a linked lists
struct arg *nth_arg(struct arg *car, int n)
{
  int cnt = 1;
  struct arg *iter;
  iter = car;
  while(iter->next != NULL) {  
    if (cnt == n) {
      return iter;
    } 
    iter = iter->next;
    cnt++;
  } 
  iter = NULL;
  return iter;
}


// #####################################################################################
// B A S I C - P R O M P T I N G - A N D - C O M M A N D S
// #####################################################################################

// =================================================
// Utility Methods for Prompting and Signal Handling
// =================================================

// Shell Prompt
void prompt()
{
  char pwd[1024];

  if (getcwd(pwd, sizeof(pwd)) == NULL) {
    perror("getcwd() error");
  }

  if (custom_prompt != NULL) {
    printf("%s@%s:%s$ ", getlogin(), custom_prompt, pwd); 
  } else {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == -1) {
      perror("gethostname() error");
    } 
    printf("%s@%s:%s$ ", getlogin(), hostname, pwd); 
  } 
}

void clear() 
{
  fflush(stdout);
  printf("\033[2J\033[1;1H");
}

void handle_signal(int signo)
{
  printf("\n");
  clear();
  printf("^C- Interrupt Received. [Press enter to continue]\n");   //possibly save history?
  fflush(stdout);
}

void quit()
{
  printf("Shell Exiting...\n");   //possibly save history?
  sleep(1);
  clear();
}


// #####################################################################################
// N O N - E X E C ' D - C O M M A N D S
// #####################################################################################

// Change path of working directory
void chpath(char *path)
{
  char* pwd;
  setenv("OLDPWD", getenv("PWD"), 1);
  if (path != NULL) {
    chdir(path);
    setenv("PWD", (char *)get_current_dir_name(), 1);
  } else {
    pwd = getenv("HOME");
    chdir(pwd);
    setenv("PWD", (char *)getenv("HOME"), 1);
  } 
}


// #####################################################################################
// E X E C U T I O N : P R O C E S S - E V A L U A T I O N
// #####################################################################################


void evaluate(struct cmd *cmd)
{
  pid_t pid;
  int   status;

  // Create Args Array for Command
  struct cmd *rest;
  struct arg *iter;
  iter = cmd->argn;
  rest = cmd->next;

  // Create Args Array 1
  int a;
  int   size_1 = arg_count(iter);
  char *argv_1[size_1+1];
  for(a=0; a<size_1; a++) {
    argv_1[a] = iter->str;
    iter      = iter->next;
  } argv_1[size_1] = (char *)0; 
  
  // Initiate Fork
  if ((pid=fork()) == 0) {
    if(execvp(argv_1[0], argv_1) < 0) {
      exit(1);
    }
  } else if (pid < 0) {
    status = -1;
  } else {
    wait(&status);
    // Once the Child is finished,
    // Return back to the executor
    pre_exec(rest);
  }  
}


void redirect_read(struct cmd *cmd)
{
  pid_t pid;
  int status;
  int in, out;

  // Prepare Arg Arrays
  struct cmd *cmd1, *cmd2, *cmd3, *rest;
  struct arg *iter;
  cmd1 = cmd;
  cmd2 = cmd1->next;
  cmd3 = cmd2->next;
  rest = cmd3;
  int a, b;
  
  // Build Array 1
  int   size_1 = arg_count(cmd1->argn);
  char *argv_1[size_1];  
  iter = cmd1->argn;
  for(a=0; a<size_1; a++) {
    argv_1[a] = iter->str;
    iter      = iter->next;
  } argv_1[size_1] = (char *)0; 

  // Build Array 2
  int   size_2 = arg_count(cmd2->argn);
  char *argv_2[size_2];
  iter = cmd2->argn;
  for(b=0; b<size_2; b++) {
    argv_2[b] = iter->str;
    iter      = iter->next;
  } argv_2[size_2] = (char *)0; 

  // Attempt Fork
  if ((pid = fork()) == -1) {
    perror("fork()"); 
    exit(1);
  } else if (pid == 0) {
    
    // Prepare the infile
    in = open(argv_1[0], O_RDONLY);
    
    // If there is a complex redirect which requires 2 commands
    // example: grep this < infile > outfile
    if((cmd2->dir == REDIR_OUT) || (cmd2->dir == REDIR_OUT2)) {
      // update the pointer to remaining cmds
      rest = cmd3->next; 

      // Build Array 3
      int   size_3 = arg_count(cmd3->argn);
      char *argv_3[size_3];
      iter = cmd3->argn;
      int c;
      for(c=0; c<size_3; c++) {
	argv_3[c] = iter->str;
	iter      = iter->next;
      } argv_3[size_3] = (char *)0;    

      // Handle Writing to File
      if(cmd->dir == REDIR_OUT) {
	out = open(argv_3[0], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR 
		   | S_IRGRP | S_IWGRP | S_IWUSR);
      } else {
	out = open(argv_3[0], O_WRONLY | O_APPEND | O_TRUNC | O_CREAT, S_IRUSR 
		   | S_IRGRP | S_IWGRP | S_IWUSR);
      }

      dup2(in,  0); // replace stdin with input file
      dup2(out, 1); // replace stdoutput with output file
      close(in);
      close(out);
      execvp(argv_1[0], argv_1); 
    } else {
      dup2(in,  0); // replace stdin with input file
      close(in);
      execvp(argv_1[0], argv_1); 
    }
  } else if (pid>0){
    //pre_exec(rest);
    //printf("Command '%s' not found.\n", argv_1[0]);
    printf("%s\n", argv_2[0]);
    wait(&status);
    pre_exec(rest);
  }
}


void redirect_write(struct cmd *cmd)
{
  pid_t pid;
  int   status;
  int   fd;
  
  // Prepare Arg Arrays
  struct cmd *cmd1, *cmd2, *rest;
  struct arg *iter;
  cmd1 = cmd;
  cmd2 = cmd1->next;
  rest = cmd2->next;
  int a, b;

  // Build Array 1
  int   size_1 = arg_count(cmd1->argn);
  char *argv_1[size_1+1];
  iter = cmd1->argn;
  for(a=0; a<size_1; a++) {
    argv_1[a] = iter->str;
    iter      = iter->next;
  } argv_1[size_1] = (char *)0; 

  // Build Array 2
  int   size_2 = arg_count(cmd2->argn);
  char *argv_2[size_2];
  iter = cmd2->argn;
  for(b=0; b<size_2; b++) {
    argv_2[b] = iter->str;
    iter      = iter->next;
  } argv_2[size_2] = (char *)0; 

  // Attempt Fork
  if ((pid = fork()) == -1) {
    perror("fork()"); 
    exit(1);
  } else if (pid == 0) {
    if(cmd1->dir == REDIR_OUT){
      fd = open(argv_2[0], O_RDWR | O_CREAT, 0644);
    } else if (cmd1->dir == REDIR_OUT2) {
      fd = open(argv_2[0], O_APPEND | O_CREAT, 0644);
    }
    dup2(fd,STDOUT_FILENO);
    dup2(fd,STDERR_FILENO);
    execvp(argv_1[0], argv_1); 
    //printf("Command '%s' not found.\n", argv_1[0]);
    exit(1);
  } else if (pid > 0) {
    printf("%s\n", argv_2[0]);
    wait(&status);
  }
}


void redirect_pipe(struct cmd *cmd)
{
  pid_t pid;
  int   fd[2];
  int   status, rtval;

  // Local Variables for building Arg Arrays
  struct cmd *cmd1, *cmd2, *rest;
  struct arg *iter;
  cmd1 = cmd;
  cmd2 = cmd1->next;
  rest = cmd2->next;
  int a,b;

  // Build Array 1
  int   size_1 = arg_count(cmd1->argn);
  char *argv_1[size_1];
  iter = cmd1->argn;
  for(a=0; a<size_1; a++) {
    argv_1[a] = iter->str;
    iter      = iter->next;
  } argv_1[size_1] = (char *)0; 

  // ATTEMPT FORK
  if (pipe(fd) == -1) {
    perror("pipe() failled\n");
  }

  // BEGIN PIPING
  if((pid=fork()) == 0) { 
    // Child 
    dup2(fd[0],0);
    close(fd[1]);
    if(cmd2->dir == REDIR_PIPE) {
      // Instead of calling exec, continue piping to next
      printf("Support for pipe sequence not yet supported\n");
      //redirect_pipe(cmd2, cmd2->next);
    } else {
      // Build Array 2
      int   size_2 = arg_count(cmd2->argn);
      char *argv_2[size_2+1];
      iter = cmd2->argn;
      for(b=0; b<size_2; b++) {
	argv_2[b] = iter->str;
	iter      = iter->next;
      } argv_2[size_2] = (char *)0; 
      pre_exec(rest);
      execvp(argv_2[0], argv_2);
      dup2(fd[1],1);
    }

  } else if (pid > 0) { // if (pid > 0){
    // Parent
    close(fd[0]);
    dup2(fd[1],1);
    execvp(argv_1[0], argv_1);
    wait(&status);
  }  
}


void background(struct cmd *cmd1)
{
  pid_t pid;
  int   status;

  struct arg *iter;
  iter = cmd1->argn;
  int   size_1 = arg_count(iter);
  char *argv_1[size_1];

  int a;
  for(a=0; a<size_1; a++) {
    argv_1[a] = iter->str;
    iter      = iter->next;
  } 
  argv_1[size_1] = (char *)0; 

  if((pid=fork()) == -1) {
    perror("fork() failed for background\n");
    exit(-1); 
  } else if (pid == 0) {
    execvp(argv_1[0], argv_1);
    printf("Command '%s' not found.", argv_1[0]);
  } else {
    sleep(1);
  }
}

void pre_exec(struct cmd *cmd)
{
  if(cmd != NULL) {
    /*
    printf("Command [con]%i [opt]%i [dir]%i\n",
	   cmd->con,
	   cmd->opt,
	   cmd->dir); 
    */
    if (cmd->dir == NONE) {
      if(cmd->opt == DETATCH) {
	background(cmd);
      } else if (cmd->opt == SOURCE) {
	//source(cmdtmp);
      } else {
	evaluate(cmd);
      }
    } else if(cmd->dir == REDIR_IN) {
      redirect_read(cmd);
    } else if((cmd->dir == REDIR_OUT) || (cmd->dir == REDIR_OUT2)) {
      redirect_write(cmd);
    } else if(cmd->dir == REDIR_PIPE) {
      redirect_pipe(cmd);
    }
  } 
}


// Source command
void source(struct cmd *cmd1)
{
  /*
  pid_t pid;
  int   status;

  struct arg *iter;
  iter = cmd1->argn;
  int   size_1 = arg_count(iter);
  char *argv_1[size_1];

  int a;
  for(a=0; a<size_1; a++) {
    argv_1[a] = iter->str;
    iter      = iter->next;
  } 
  argv_1[size_1] = (char *)0; 
  */
}


// #####################################################################################
// P A R S I N G
// #####################################################################################

// parsing helper to check for terminals
int isRedirector(char *s)
{
  if (strrchr(s, '<') != NULL) {
    return REDIR_IN;
  } else if(strrchr(s, '_') != NULL) {
    return REDIR_OUT2;
  } else if (strrchr(s, '>') != NULL) {
    return REDIR_OUT;
  } else if (strrchr(s, '|') != NULL) {
    return REDIR_PIPE;
  }
  return NONE;
}


int isConnector(char *s)
{
  if (strchr(s, '+') != NULL) {
    return CONNECTOR;
  }
  return NONE;
}


int isTerminal(char *s)
{
  if(strchr(s, ';') != NULL) {
    return SOURCE;

  } else if (strchr(s, '&') != NULL) {
    return DETATCH;
  } 
  return NONE;
}


// Check whether a token is (< > | _ & ;)
int isMetachar(char *s)
{
  return (isRedirector(s) || 
	  isTerminal(s)   || 
	  isConnector(s));
}


int isMetachar2(char *s)
{
  return (isRedirector(s) || 
	  isTerminal(s));
}


// Parser
struct cmd *parse(char *line)
{
  // Declare pointers to head, last, and tmp
  // pointers for cmd list and arg list
  struct cmd *cmdlast = NULL;
  struct cmd *cmdhead, *cmdtmp;
  struct arg *arglast = NULL;
  struct arg *arghead, *argtmp;

  // Allocate space for the Head of the cmd and arg lists
  cmdhead = (struct cmd *) malloc(sizeof(struct cmd));
  arghead = (struct arg *) malloc(sizeof(struct arg));

  char *token = NULL;
  token = strtok(line, " \n"); // get the first token
  
  arghead->str  = token;   // set the head of argument to token
  arglast       = arghead; // update last pntr
  cmdhead->argn = arglast; // assign arg head to cmd
  cmdhead->opt  = NONE;    // give cmd opt default val
  cmdhead->dir  = NONE;    // redirection type (0 default)
  cmdhead->con  = NONE;    // redirection type (0 default)
  cmdlast       = cmdhead; // set last to the current head

  if (token != NULL) { 
    // check if command is ls:
    // special case to display ls with color
    if (strcmp(token, "ls") == 0) {
      argtmp = (struct arg *) malloc(sizeof(struct arg));
      argtmp->str   = "--color=auto";
      argtmp->next  = NULL;
      arglast->next = argtmp;
      arglast       = argtmp;
    }
  }

  // While tokens still remain . . .
  while (token != NULL) {

    // Get next token
    token = strtok(NULL, " \n");

    // Process token for redirectors and terminal metachars
    if(token != NULL) {
      if(isMetachar(token)) {
	// update options: opt (& ;) dir (> < | _) and con (+)
	// on current last node BEFORE creating linking a new command
	cmdlast->opt = isTerminal(token);
	cmdlast->dir = isRedirector(token);
	cmdlast->con = isConnector(token);
	
	// Get next token
	token = strtok(NULL, " \n");

	// if there's a token after metachar 
	if(token != NULL) {
	  
	  // Allocate Space for this new next command
	  cmdtmp = (struct cmd *) malloc(sizeof(struct cmd));	  
	  cmdtmp->next   = NULL;   // tmp points to NULL
	  cmdlast->next  = cmdtmp; // take our old last node and make its next our new added node
	  cmdlast        = cmdtmp; // SHIFT 'last' pntr from prev last to new last (tmp)

	  // Allocate Space for a new arg head for new cmd node
	  argtmp = (struct arg *) malloc(sizeof(struct arg));
	  argtmp->str   = token;
	  argtmp->next  = NULL;
	  arglast       = argtmp;  // same as arglast = arghead, redefining head
	  cmdlast->argn = arglast; // plant this as the head node for the current last cmd
	  //arglast->next = argtmp; // SHIFT 'last' pntr from prev last to new last (tmp)

	  // check if command is ls:
	  // special case to display ls with color
	  if (strcmp(token, "ls") == 0) {
	    argtmp = (struct arg *) malloc(sizeof(struct arg));
	    argtmp->str   = "--color=auto";
	    argtmp->next  = NULL;
	    arglast->next = argtmp;
	    arglast       = argtmp;
	  }
	}
      } else if (token != NULL) {
	// Allocate Space to add another arg to current cmd
	argtmp = (struct arg *) malloc(sizeof(struct arg));
	argtmp->str   = token;
	argtmp->next  = NULL;
	arglast->next = argtmp;
	arglast       = argtmp;
      }
    }
  }

  /* For Debugging 
  int a=0;
  int b=0;
  cmdtmp = cmdhead;
  while(cmdtmp != NULL) {
    argtmp=cmdtmp->argn;
    while(argtmp != NULL) {
      printf("[cmd]%i [arg]%i [val]%s\n", a,b,argtmp->str);
      argtmp=argtmp->next;
      b++;
    }
    cmdtmp=cmdtmp->next;
    a++;
    }*/

  return cmdhead;
}


// Process the formatted command
char *proc(struct cmd *cmds)
{
  struct arg *args;
  args = cmds->argn;

  if (args->str == NULL) {
    args->str = "reset";
  } else if (*(args->str) == '\x0C') {
    clear();
  } else if (strcmp(args->str, "cd") == 0) {
    chpath((args->next)->str);
  } else if (args->str != NULL) {
    pre_exec(cmds);
  } 
  return args->str;
}


// Replace all instances of a substring with another substring
char *replace(const char *str, const char *old, const char *new)
{
  char *ret, *r;
  const char *p, *q;
  size_t oldlen = strlen(old);
  size_t count, retlen, newlen = strlen(new);

  if(strstr(str, old) == -1) {
    return str;
  }
  
  if (oldlen != newlen) {
    for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
      count++;
    } /* this is undefined if p - str > PTRDIFF_MAX */
    retlen = p - str + strlen(p) + count * (newlen - oldlen);
  } else {
    retlen = strlen(str);
  }
  
  ret = malloc(retlen + 1);
  
  for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
    /* this is undefined if q - p > PTRDIFF_MAX */
    ptrdiff_t l = q - p;
    memcpy(r, p, l);
    r += l;
    memcpy(r, new, newlen);
    r += newlen;
  }
  strcpy(r, p);
  return ret;
}


// Inject spaces before and after metachars to assist in tokenization
char *preproc(char *line) 
{
  char *orig[7] = { "&&",  "&" ,  ">>" ,  "<" ,  ">" ,  ";" ,  "|" };
  char *repl[7] = {" + ", " & ", " _ " , " < ", " > ", " ; ", " | "};
  int op;  
  for(op=0; op<7; op++) {      
    line = replace(line, orig[op], repl[op]);
  } 
  return line;
}


// Main Method for Shell Initialization
int main(int argc, char *argv[], char *envp[]) 
{  
  int   linebytes;
  int   nbytes  = 100;
  char *line;
  char *fcmd; //first command string
  custom_prompt = argv[1];
  struct cmd *cmds;

  clear(); // flush screen clear and print welcome

  printf("Welcome to seashell!\n");

  // Initiate signal handling listeners
  signal(SIGINT, SIG_IGN);
  signal(SIGINT, handle_signal);
  
  do {
    prompt();
    line  = (char *) malloc (nbytes + 1);
    linebytes = getline(&line, &nbytes, stdin);    
    line = preproc(line);
    cmds = parse(line);
    fcmd = proc(cmds);
  } while (strcmp(fcmd,   "exit")!=0 && 
	   strcmp(fcmd, "logout")!=0 &&
	   strcmp(fcmd, "")!= 0);

  //freeing of vars should be done here
  free(cmds);
  quit();
  return 0;
}


