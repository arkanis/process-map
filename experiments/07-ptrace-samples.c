#define _GNU_SOURCE  // For asprintf
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>


typedef struct {
	pid_t pid;
} Task;

int LoadTaskTraces(char* pid, Task** tasks, size_t* taskCount);
void UnloadTaskTraces(Task* tasks, size_t taskCount, int signalFd);

void UpdateTaskTracesViaSignalFd(Task* tasks, size_t taskCount, int signalFd);
void UpdateTaskTracesWaitPidSequence(Task* tasks, size_t taskCount, int signalFd);
void UpdateTaskTracesWaitPidPipelined(Task* tasks, size_t taskCount, int signalFd);


int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s pid\n", argv[0]);
		return 1;
	}
	
	Task* tasks = NULL;
	size_t taskCount = 0;
	int signalFd = LoadTaskTraces(argv[1], &tasks, &taskCount);
	
	for (size_t i = 0; i < 10; i++) {
		sleep(1);
		UpdateTaskTracesWaitPidPipelined(tasks, taskCount, signalFd);
	}
	
	UnloadTaskTraces(tasks, taskCount, signalFd);
	return 0;
}

int LoadTaskTraces(char* pid, Task** tasks, size_t* taskCount) {
	*tasks = NULL;
	*taskCount = 0;
	
	char* tasksDirPath = NULL;
	asprintf(&tasksDirPath, "/proc/%s/task", pid);
	DIR* tasksDir = opendir(tasksDirPath);
	free(tasksDirPath);
	if (tasksDir == NULL)
		return -1;
	
	struct dirent *entry;
	while ((entry = readdir(tasksDir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		
		pid_t pid = atoi(entry->d_name);
		
		if ( ptrace(PTRACE_SEIZE, pid, NULL, NULL) != 0 )
			perror("ptrace(PTRACE_SEIZE)");
		
		*taskCount += 1;
		*tasks = realloc(*tasks, *taskCount * sizeof(*tasks[0]));
		(*tasks)[*taskCount - 1] = (Task){ .pid = pid };
	}
	closedir(tasksDir);
	
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGTRAP);
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		perror("sigprocmask()");
	
	int signalFd = signalfd(-1, &mask, 0);
	return signalFd;
}

void UpdateTaskTracesViaSignalFd(Task* tasks, size_t taskCount, int signalFd) {
	size_t interruptedCount = 0;
	for (size_t t = 0; t < taskCount; t++) {
		if ( ptrace(PTRACE_INTERRUPT, tasks[t].pid, NULL, NULL) == 0 )
			interruptedCount++;
		else
			perror("ptrace(PTRACE_INTERRUPT)");
	}
	printf("LOG: interrupted %zu tasks\n", interruptedCount);
	
	while (interruptedCount > 0) {
		struct signalfd_siginfo signals[interruptedCount];
		ssize_t readBytes = read(signalFd, &signals, sizeof(signals));
		printf("    read %zu signals\n", readBytes / sizeof(signals[0]));
		for (size_t s = 0; s < readBytes / sizeof(signals[0]); s++) {
			struct user_regs_struct regs;
			if ( ptrace(PTRACE_GETREGS, signals[s].ssi_pid, NULL, &regs) != 0 )
				perror("ptrace(PTRACE_GETREGS)");
			if ( ptrace(PTRACE_CONT, signals[s].ssi_pid, NULL, NULL) != 0 )
				perror("ptrace(PTRACE_CONT)");
			interruptedCount--;
			
			Task* task = NULL;
			for (size_t t = 0; t < taskCount; t++) {
				if (tasks[t].pid == (pid_t)signals[s].ssi_pid) {
					task = &tasks[t];
					break;
				}
			}
			printf("    task %u sample\n", task->pid);
			
			// Add samples to vertex buffer
		}
	}
}

void UpdateTaskTracesWaitPidSequence(Task* tasks, size_t taskCount, int signalFd) {
	(void)signalFd;
	printf("LOG: sampling %zu tasks\n", taskCount);
	for (size_t t = 0; t < taskCount; t++) {
		struct user_regs_struct regs;
		if ( ptrace(PTRACE_INTERRUPT, tasks[t].pid, NULL, NULL) != 0 )  perror("ptrace(PTRACE_INTERRUPT)");
		if ( waitpid(tasks[t].pid, NULL, 0) == -1 )                     perror("waitpid()");
		if ( ptrace(PTRACE_GETREGS, tasks[t].pid, NULL, &regs) != 0 )   perror("ptrace(PTRACE_GETREGS)");
		if ( ptrace(PTRACE_CONT, tasks[t].pid, NULL, NULL) != 0 )       perror("ptrace(PTRACE_CONT)");
		printf("    task %u sample\n", tasks[t].pid);
	}
}

void UpdateTaskTracesWaitPidPipelined(Task* tasks, size_t taskCount, int signalFd) {
	(void)signalFd;
	printf("LOG: sampling %zu tasks\n", taskCount);
	for (size_t t = 0; t < taskCount; t++) {
		if ( ptrace(PTRACE_INTERRUPT, tasks[t].pid, NULL, NULL) != 0 )  perror("ptrace(PTRACE_INTERRUPT)");
	}
	for (size_t t = 0; t < taskCount; t++) {
		struct user_regs_struct regs;
		if ( waitpid(tasks[t].pid, NULL, 0) == -1 )                     perror("waitpid()");
		if ( ptrace(PTRACE_GETREGS, tasks[t].pid, NULL, &regs) != 0 )   perror("ptrace(PTRACE_GETREGS)");
		if ( ptrace(PTRACE_CONT, tasks[t].pid, NULL, NULL) != 0 )       perror("ptrace(PTRACE_CONT)");
		printf("    task %u sample\n", tasks[t].pid);
	}
}

void UnloadTaskTraces(Task* tasks, size_t taskCount, int signalFd) {
	for (size_t i = 0; i < taskCount; i++) {
		if ( ptrace(PTRACE_DETACH, tasks[i].pid, NULL, NULL) != 0 )
			perror("ptrace(PTRACE_DETACH)");
	}
	
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
		perror("sigprocmask()");
	
	close(signalFd);
}