/**
	* Shell framework
	* course Operating Systems
	* Radboud University
	* v22.09.05

	Student names:
	- ...
	- ...
*/

/**
 * Hint: in most IDEs (Visual Studio Code, Qt Creator, neovim) you can:
 * - Control-click on a function name to go to the definition
 * - Ctrl-space to auto complete functions and variables
 */

// function/class definitions you are going to use
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sstream>
#include <vector>
#include <list>
#include <optional>
#include <numeric> //accumulate

// although it is good habit, you don't have to type 'std' before many objects by including this line
using namespace std;

struct Command {
  vector<string> parts = {};
};

struct Expression {
  vector<Command> commands;
  string inputFromFile;
  string outputToFile;
  bool background = false;
};

// Parses a string to form a vector of arguments. The separator is a space char (' ').
vector<string> split_string(const string& str, char delimiter = ' ') {
  vector<string> retval;
  for (size_t pos = 0; pos < str.length(); ) {
    // look for the next space
    size_t found = str.find(delimiter, pos);
    // if no space was found, this is the last word
    if (found == string::npos) {
      retval.push_back(str.substr(pos));
      break;
    }
    // filter out consequetive spaces
    if (found != pos)
      retval.push_back(str.substr(pos, found-pos));
    pos = found+1;
  }
  return retval;
}

// wrapper around the C execvp so it can be called with C++ strings (easier to work with)
// always start with the command itself
// DO NOT CHANGE THIS FUNCTION UNDER ANY CIRCUMSTANCE
int execvp(const vector<string>& args) {
  // build argument list
  const char** c_args = new const char*[args.size()+1];
  for (size_t i = 0; i < args.size(); ++i) {
    c_args[i] = args[i].c_str();
  }
  c_args[args.size()] = nullptr;
  // replace current process with new process as specified
  int rc = ::execvp(c_args[0], const_cast<char**>(c_args));
  // if we got this far, there must be an error
  int error = errno;
  // in case of failure, clean up memory (this won't overwrite errno normally, but let's be sure)
  delete[] c_args;
  errno = error;
  return rc;
}

// Executes a command with arguments. In case of failure, returns error code.
int execute_command(const Command& cmd) {
  auto& parts = cmd.parts;
  if (parts.size() == 0)
    return EINVAL;

  // execute external commands
  int retval = execvp(parts);
  return retval;
}

void display_prompt() {
  char buffer[512];
  char* dir = getcwd(buffer, sizeof(buffer));
  if (dir) {
    cout << "\e[32m" << dir << "\e[39m"; // the strings starting with '\e' are escape codes, that the terminal application interpets in this case as "set color to green"/"set color to default"
  }
  cout << "$ ";
  flush(cout);
}

string request_command_line(bool showPrompt) {
  if (showPrompt) {
    display_prompt();
  }
  string retval;
  getline(cin, retval);
  return retval;
}

// note: For such a simple shell, there is little need for a full-blown parser (as in an LL or LR capable parser).
// Here, the user input can be parsed using the following approach.
// First, divide the input into the distinct commands (as they can be chained, separated by `|`).
// Next, these commands are parsed separately. The first command is checked for the `<` operator, and the last command for the `>` operator.
Expression parse_command_line(string commandLine) {
  Expression expression;
  vector<string> commands = split_string(commandLine, '|');
  for (size_t i = 0; i < commands.size(); ++i) {
    string& line = commands[i];
    vector<string> args = split_string(line, ' ');
    if (i == commands.size() - 1 && args.size() > 1 && args[args.size()-1] == "&") {
      expression.background = true;
      args.resize(args.size()-1);
    }
    if (i == commands.size() - 1 && args.size() > 2 && args[args.size()-2] == ">") {
      expression.outputToFile = args[args.size()-1];
      args.resize(args.size()-2);
    }
    if (i == 0 && args.size() > 2 && args[args.size()-2] == "<") {
      expression.inputFromFile = args[args.size()-1];
      args.resize(args.size()-2);
    }
    expression.commands.push_back({args});
  }
  return expression;
}

int execute_expression(Expression& expression) {

  // Check for empty expression
  if (expression.commands.size() == 0)
    return EINVAL;

  // Check for empty commands:
  for (Command command : expression.commands) {
    if (command.parts.empty()) {
      fprintf(stderr, "Syntax error: empty command in chain.\n");
      return 0;
    }
  }

  // Handle intern commands (like 'cd' and 'exit')

  // Check for intern command `exit`:
  for (size_t i = 0 ; i < expression.commands.size(); i++) {
    if (expression.commands[i].parts[0] == "exit") {
      // Check for `exit`s in a chain:
      if (expression.commands.size() > 0) {
        fprintf(stderr, "Semantic error: exit in a chain of commands.\n");
        return 0;
      } else if (!expression.inputFromFile.empty() || !expression.outputToFile.empty()) { 
        fprintf(stderr, "Semantic error: cannot redirect input or output in exit.\n");
        return 0;
      } else if (expression.commands[i].parts.size() > 1) {
        fprintf(stderr, "Syntax error: exit cannot take arguments.\n");
        return 0;
      } else {
        exit(0);
      }
    }
  }

  // if one of the first starts with `cd` and has one argument,
  // the shell changes the working directory to that argument using `cd`
  if (expression.commands[0].parts[0] == "cd") {
    // Check for syntax errors on `cd` (not exactly 1 argument):
    if (expression.commands[0].parts.size() != 2) {
      fprintf(stderr, "Syntax error: cd takes one argument.\n");
      return 0;
    } else { // change directory
      const char *path = expression.commands[0].parts[1].c_str();
      if (-1 == chdir(path)) {
        // if we could not change the working directory, we should raise an error and return.
        // otherwise we might execute commands in the unintended directory
        fprintf(stderr, "cd: %s: no such file or directory.\n", path);
        return 0;
      }
      // if succesful, the `cd` command should disappear from the expression or else
      // the children will try to execute it and fail.
      expression.commands.erase(expression.commands.begin());
    }
  }
 



  // create vector of children pids:
  vector<pid_t> children;

  // create a vector of pipes:
  // child i and i+1 communicate via pipes[i]
  // child i needs to close read and write ends
  // of pipes[0..i-2] and write end of pipes[i-1]
  // and read end of pipes[i]. the other pipes
  // are not yet created when the i-th child is
  // forked.
  int pipes[expression.commands.size()][2];
  
  
  // External commands, executed with fork():
  // Loop over all commandos, and connect the output and input of the forked processes
  for (size_t i = 0; i < expression.commands.size(); i++) {

    // Create a new pipe for every newly forked process
    if (-1 == pipe(pipes[i])) {
      fprintf (stderr, "Could not create pipe for %lud-th chain.", i);
      exit(1);
    }

    // Fork the i-th child
    pid_t child_i = fork();
    if (child_i == -1) {
      fprintf(stderr, "Could not fork %lud-th child.", i);
      exit(1);
    }

    if (child_i != 0) {
      children.push_back(child_i);
    } else {
      // For children, we have three cases:
      // 1. the 0-th child in the chain should either get inputFromFile or input from  STDIN_FILENO.
      //    it should put output into (pipes[0])[1]
      // 2. the last child n in the chain should either put outputToFile or output to  STDOUT_FILENO.
      //    it should get input from (pipes[n-1])[0]
      // 3. all children i in between should put output to (pipes[i])[1] and get input from (pipes[i])[0] 



      if (i != 0) {
        // all children except the 0th read from 
        // the preceding pipe:
        dup2(pipes[i-1][0], STDIN_FILENO);
        close(pipes[i-1][1]);
      }

      if (i != expression.commands.size() - 1) {
        // all children except the last write to
        // the next pipe
        dup2(pipes[i][1], STDOUT_FILENO);
        close(pipes[i][0]);
      }

      // close all the pipes[0, ..., i-2], if any:
      // this is freeing non-used resources to make it
      // possible for reading ends of pipes to sense an EOF.
      for(size_t j = 0; j+1 < i; j ++){ // note the comparison j+1 < i, to prevent underflow errors for size_t
        close(pipes[j][0]);
        close(pipes[j][1]);
        // we do not check for errors, because if they occur, the parent will wait for the
        // child and thus kill it anyway.
      }
      
      if (i == 0 && !expression.inputFromFile.empty()) {
          const char *path = expression.inputFromFile.c_str();
          int fd_file = open(path, O_RDONLY);
          dup2(fd_file, STDIN_FILENO);
      }

      if (i == 0 && expression.inputFromFile.empty() && expression.background) {
        close(STDIN_FILENO);
      }
      
      if (i == expression.commands.size() - 1 && !expression.outputToFile.empty()) {
          const char *path = expression.outputToFile.c_str();
          int fd_file = open(path, O_WRONLY | O_CREAT, 0666); // if the file does not exist, create it (O_CREAT flag).
          dup2(fd_file, STDOUT_FILENO);
      }

      execute_command(expression.commands[i]);

      // display nice warning that the executable could not be found
      fprintf(stderr, "Could not find: %s\n", expression.commands[i].parts[0].c_str());
      abort(); // if the executable is not found, we should abort. (why?)
    }
  }

  // the parent process needs to free non-used resources:
  for (auto pipe : pipes) {
    close(pipe[0]);
    close(pipe[1]);
  }

  // the parent process awaits termination of all children
  // which were stored in the vector children.
  // this is done unless the expression is executed in
  // background.
  if (!expression.background) {
    for (pid_t child : children) {
      waitpid(child, nullptr, 0);
    }
  }


  return 0;
}


// framework for executing "date | tail -c 5" using raw commands
// two processes are created, and connected to each other
int step1(bool showPrompt) {
  // create communication channel shared between the two processes
  int pipefd[2];
  
  if (pipe(pipefd) < 0) {
    printf("error: could not open pipe\n");
    exit(1);
  }

  pid_t child1 = fork();
  if (child1 == 0) {
    // redirect standard output (STDOUT_FILENO) to the input of the shared communication channel
    dup2(pipefd[1], STDOUT_FILENO);
    
    // free non used resources (why?)
    close(pipefd[0]); // is this "freeing non used resources"?
    
    Command cmd = {{string("date")}};
    execute_command(cmd);
    // display nice warning that the executable could not be found
    printf("error: could not find executable: %s\n", "date");
    abort(); // if the executable is not found, we should abort. (why?)
  }

  pid_t child2 = fork();
  if (child2 == 0) {
    // redirect the output of the shared communication channel to the standard input (STDIN_FILENO).
    dup2(pipefd[0], STDIN_FILENO);
    
    // free non used resources (why?)
    close(pipefd[1]);
    
    Command cmd = {{string("tail"), string("-c"), string("5")}};
    execute_command(cmd);
    // display nice warning that the executable could not be found
    printf("error: could not find executable:%s\n", "tail -c 5");
    abort(); // if the executable is not found, we should abort. (why?)
  }

  // free non used resources (why?)
  // If we do not close the ends of the pipe in a process that won't use them,
  // the reading process will not detect end-of-files, and the program will 
  // hang.
  close(pipefd[0]);
  close(pipefd[1]);
  
  // wait on child processes to finish (why both?)
  // According to manual 
  //    "In the case of a terminated child, performing a wait allows
  //     the system to release the resources associated with  the  child;  if  a
  //     wait  is not performed, then the terminated child remains in a "zombie"
  //     state (see NOTES below)."
  // So I think it is to ensure that the child processes actually finished.
  // But then my question would be (and I hope you can provide feedback on this):
  // What do we do if the wait blocks forever? Because even then we cannot 
  // kill the children from the parent process (since the parent process 
  // is blocking and also since killing other processes in user mode is
  // not really allowed for
  // security reasons)..
  waitpid(child1, nullptr, 0);
  waitpid(child2, nullptr, 0);
  return 0;
}

int shell(bool showPrompt) {
  //* <- remove one '/' in front of the other '/' to switch from the normal code to step1 code
  while (cin.good()) {
    string commandLine = request_command_line(showPrompt);
    Expression expression = parse_command_line(commandLine);
    int rc = execute_expression(expression);
    if (rc != 0)
      cerr << strerror(rc) << endl;
  }
  return 0;
  /*/
  return step1(showPrompt);
  //*/
}
