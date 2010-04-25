/*
 * lat_mq.c - pipe transaction test
 *
 * usage: lat_mq [-P <parallelism>] [-W <warmup>] [-N <repetitions>]
 *
 * based on lat_pipe.c
 */
char	*id = "$Id$\n";

#include "bench.h"
#include <mqueue.h>

void initialize(iter_t iterations, void *cookie);
void cleanup(iter_t iterations, void *cookie);
void doit(iter_t iterations, void *cookie);
void writer(int w, int r);

typedef struct _state {
	int	pid;
    mqd_t mq;
} state_t;

int 
main(int ac, char **av)
{
	state_t state;
	int parallel = 1;
	int warmup = 0;
	int repetitions = TRIES;
	int c;
	char* usage = "[-P <parallelism>] [-W <warmup>] [-N <repetitions>]\n";

	while (( c = getopt(ac, av, "P:W:N:")) != EOF) {
		switch(c) {
		case 'P':
			parallel = atoi(optarg);
			if (parallel <= 0) lmbench_usage(ac, av, usage);
			break;
		case 'W':
			warmup = atoi(optarg);
			break;
		case 'N':
			repetitions = atoi(optarg);
			break;
		default:
			lmbench_usage(ac, av, usage);
			break;
		}
	}
	if (optind < ac) {
		lmbench_usage(ac, av, usage);
	}

	state.pid = 0;

	benchmp(initialize, doit, cleanup, SHORT, parallel, 
		warmup, repetitions, &state);
	micro("mq latency", get_n());
	return (0);
}

void 
initialize(iter_t iterations, void* cookie)
{
	char	c;
	state_t * state = (state_t *)cookie;

	if (iterations) return;

    state->mq = mq_open("testqueue", O_CREAT | O_EXCL);
    if (state->mq == -1) {
        perror("mq_open");
        return;
    }

	handle_scheduler(benchmp_childid(), 0, 1);
	switch (state->pid = fork()) {
	    case 0:
		handle_scheduler(benchmp_childid(), 1, 1);
		signal(SIGTERM, exit);
		childloop(state->mq);
		return;

	    case -1:
		perror("fork");
		return;

	    default:
		break;
	}

	/*
	 * One time around to make sure both processes are started.
	 */
	if (write(state->p1[1], &c, 1) != 1 || read(state->p2[0], &c, 1) != 1){
		perror("(i) read/write on pipe");
		exit(1);
	}
}

void 
cleanup(iter_t iterations, void* cookie)
{
	state_t * state = (state_t *)cookie;

	if (iterations) return;

	if (state->pid) {
		kill(state->pid, SIGKILL);
		waitpid(state->pid, NULL, 0);
		state->pid = 0;
	}
}

void 
doit(register iter_t iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	char		c;
	register char	*cptr = &c;
    register mqd_t * mq = state->mq;

	while (iterations-- > 0) {
		if (mq_send(mq, cptr, sizeof(c), 0) != 1 ||
		    mq_receive(mq, cptr, sizeof(c), NULL) != 1) {
			perror("(r) receive/send on mq");
			exit(1);
		}
	}
}

void 
writer(mqd_t mq)
{
	char		c;
	register char	*cptr = &c;

	for ( ;; ) {
		if (mq_receive(mq, cptr, sizeof(c), NULL) != 1 ||
			mq_send(mq, cptr, sizeof(c), 0) != 1) {
			    perror("(w) receive/send on mq");
		}
	}
}
