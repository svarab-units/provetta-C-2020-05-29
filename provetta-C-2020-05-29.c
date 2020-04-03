#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define handle_error_en(en, msg) \
	   do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
	   do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define NUMBER_OF_MESSAGES 4096
int pipe_fd[2]; // pipe

void writemsg2stdout(char * msg) {
	write(STDOUT_FILENO, msg, strlen(msg) );
}

void parent_proc() {
	// Seed random
	srand(1000);

	// Master process
	close(pipe_fd[0]);

	// Generate
	for (int i = 0; i < NUMBER_OF_MESSAGES; i++) {
		int op1 = rand();
		write(pipe_fd[1], &op1, sizeof(op1));
		int op2 = rand();
		write(pipe_fd[1], &op2, sizeof(op2));
	}

	close(pipe_fd[1]);		// Close pipe
	wait(NULL);				// Wait for child
	printf("[PARENT] Ok");

	// Call sha512sum
	char *args[] = {"/usr/bin/sha512sum", "output.txt"};
	execv("/usr/bin/sha512sum", args);

	exit(EXIT_SUCCESS);
}

static void * thread_function_diff(void *arg)
{
	int *op = *((int*)arg);
	int res = op[0] - op[1];
	return &res;
}

void child_proc() {
	// Child process
	close(pipe_fd[1]);

	// Open file
	int fd = open("output.txt", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		writemsg2stdout("[CHILD] File open failed");
		exit(EXIT_FAILURE);
	}

	// Do job
	int op1 = 0;
	int op2 = 0;
	int numRead1 = -1, numRead2 = -1;
	while(1) {
		// Read from pipe
		numRead1 = read(pipe_fd[0], &op1, sizeof(op1));
		numRead2 = read(pipe_fd[0], &op2, sizeof(op2));

		// Check
		if (numRead1 == -1 || numRead2 == -1) {
			perror("[CHILD] Pipe read failed");
			exit(EXIT_FAILURE);
		} else if (numRead1 == 0 || numRead2 == 0) {
			// Reading only of op1 or op2 shall not happen
			break;
		}

		/*
		// Write to file
		op1 = op1 - op2;
		numRead1 = write(fd, &op1, sizeof(op1));
		*/

		// Threaded
		int op[] = {op1, op2};
		int* res;

		int s;
		pthread_t thread_id;
		s = pthread_create(&thread_id, NULL, &thread_function_diff, &op);
		if (s != 0) handle_error_en(s, "[maint] pthread_create");
		s = pthread_join(thread_id, res);
		if (s != 0) handle_error_en(s, "[maint] pthread_join");
		numRead1 = write(fd, res, sizeof(int));

		// Check
		if (numRead1 == -1) {
			perror("[CHILD] File write failed");
			exit(EXIT_FAILURE);
		}
	}

	// Close file
	if (close(fd) == -1) {
		perror("[CHILD] File close failed");
		exit(EXIT_FAILURE);
	}

	// Done
	close(pipe_fd[0]);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
	// Create pipe
	// pipe_fd[0] - READ
	// pipe_fd[1] - write
	if (pipe(pipe_fd) == -1) {
		perror("Pipe creation failed");
		exit(EXIT_FAILURE);
	}

	// Fork
	switch (fork()) {
		case -1:
			perror("Fork failed");
			exit(EXIT_FAILURE);
		case 0:
			child_proc();
			break; // just for code error checking
		default:
			parent_proc();
			break; // just for code error checking
	}
}
