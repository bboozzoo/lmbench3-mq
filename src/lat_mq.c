/*
 * lat_mq.c - pipe transaction test
 *
 * usage: lat_mq [-P <parallelism>] [-W <warmup>] [-N <repetitions>]
 *
 * based on lat_pipe.c
 */
char	*id = "$Id$\n";
char buf[8192] = {0};

#include "bench.h"
#include <mqueue.h>

void initialize(iter_t iterations, void *cookie);
void cleanup(iter_t iterations, void *cookie);
void doit(iter_t iterations, void *cookie);
void childloop(mqd_t mq);

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
    struct mq_attr attr;


	if (iterations) return;

    memset(&attr, 0, sizeof(attr));
    attr.mq_flags = 0;
    attr.mq_maxmsg = 1;
    attr.mq_msgsize = 1;
    attr.mq_curmsgs = 0;
    state->mq = mq_open("/testqueue", O_CREAT | O_RDWR, 0666, &attr);
    if (state->mq == -1) {
        perror("mq_open");
        return;
    }
    fprintf(stderr, "mq fd: %d\n", state->mq);
    mq_getattr(state->mq, &attr);
    fprintf(stderr, "attrs\n");
    fprintf(stderr, "flags: 0x%x\n", attr.mq_flags);
    fprintf(stderr, "maxmsg: %ld\n", attr.mq_maxmsg);
    fprintf(stderr, "msgsize: %ld\n", attr.mq_msgsize);
    fprintf(stderr, "curmsg: %ld\n", attr.mq_curmsgs);
    attr.mq_msgsize = 1;
    attr.mq_maxmsg = 1;
    if (mq_setattr(state->mq, &attr, NULL) == -1) {
        perror("mq_setattr");
        return;
    }
    mq_getattr(state->mq, &attr);

    fprintf(stderr, "attrs\n");
    fprintf(stderr, "flags: 0x%x\n", attr.mq_flags);
    fprintf(stderr, "maxmsg: %ld\n", attr.mq_maxmsg);
    fprintf(stderr, "msgsize: %ld\n", attr.mq_msgsize);
    fprintf(stderr, "curmsg: %ld\n", attr.mq_curmsgs);
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
#if 1
    {
        int snd = 0;
        int rcv = 0;
        snd = mq_send(state->mq, &c, 1, 0);
        rcv = mq_receive(state->mq, buf, sizeof(buf), NULL);
        fprintf(stderr, "snd: %d rcv: %d\n", snd, rcv);
        /*
	if (mq_send(state->mq, &c, 1, 0) != -1 || mq_receive(state->mq, buf, sizeof(buf), NULL) != 1){
		perror("(i) read/write on mq");
		exit(1);
	} */
    }
#endif
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
    mq_close(state->mq);
    mq_unlink("/testqueue");
}

void 
doit(register iter_t iterations, void *cookie)
{
	state_t *state = (state_t *) cookie;
	char		c;
	register char	*cptr = &c;
    register mqd_t mq = state->mq;

	while (iterations-- > 0) {
		if (mq_send(mq, cptr, sizeof(c), 0) == -1 ||
		    mq_receive(mq, buf, sizeof(buf), NULL) == -1) {
			perror("(r) receive/send on mq");
            mq_close(mq);
			exit(1);
		}
	}
}

void 
childloop(mqd_t mq)
{
	char		c;
	register char	*cptr = &c;

	for ( ;; ) {
		if (mq_receive(mq, buf, sizeof(buf), NULL) == -1 ||
			mq_send(mq, cptr, sizeof(c), 0) == -1) {
			    perror("(w) receive/send on mq");
                mq_close(mq);
                exit(1);
		}
	}
}
