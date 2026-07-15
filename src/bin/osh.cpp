#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

#define SHELL_LINE_MAX 128
#define SHELL_PATH_MAX 256
#define SHELL_MAX_ARGS 16
#define SHELL_MAX_JOBS 16
#define SHELL_PATH_DIRS 2

#define JOB_UNDEF 0
#define JOB_FG 1
#define JOB_BG 2

struct Job {
    int pid;
    int jid;
    int state;
    char cmdline[SHELL_LINE_MAX];
};

static Job jobs[SHELL_MAX_JOBS];
static int next_jid = 1;

static void clear_job(Job* job)
{
    job->pid = 0;
    job->jid = 0;
    job->state = JOB_UNDEF;
    job->cmdline[0] = '\0';
}

static void init_jobs()
{
    for (int i = 0; i < SHELL_MAX_JOBS; i++) {
        clear_job(&jobs[i]);
    }
}

static int max_jid()
{
    int max = 0;
    for (int i = 0; i < SHELL_MAX_JOBS; i++) {
        if (jobs[i].jid > max) {
            max = jobs[i].jid;
        }
    }
    return max;
}

static Job* get_job_pid(int pid)
{
    for (int i = 0; i < SHELL_MAX_JOBS; i++) {
        if (jobs[i].pid == pid) {
            return &jobs[i];
        }
    }
    return nullptr;
}

static Job* get_job_jid(int jid)
{
    for (int i = 0; i < SHELL_MAX_JOBS; i++) {
        if (jobs[i].jid == jid) {
            return &jobs[i];
        }
    }
    return nullptr;
}

static bool add_job(int pid, int state, const char* cmdline)
{
    if (pid <= 0) {
        return false;
    }
    for (int i = 0; i < SHELL_MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = next_jid++;
            if (next_jid > SHELL_MAX_JOBS) {
                next_jid = 1;
            }
            strcpy(jobs[i].cmdline, cmdline);
            return true;
        }
    }
    printf("[shell] too many jobs\n");
    return false;
}

static bool delete_job(int pid)
{
    Job* job = get_job_pid(pid);
    if (!job) {
        return false;
    }
    clear_job(job);
    next_jid = max_jid() + 1;
    if (next_jid <= 0) {
        next_jid = 1;
    }
    return true;
}

static void list_jobs()
{
    for (int i = 0; i < SHELL_MAX_JOBS; i++) {
        if (jobs[i].pid == 0) {
            continue;
        }
        printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
        if (jobs[i].state == JOB_BG) {
            printf("Running ");
        } else if (jobs[i].state == JOB_FG) {
            printf("Foreground ");
        } else {
            printf("Unknown ");
        }
        printf("%s\n", jobs[i].cmdline);
    }
}

static void trim_newline(char* s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static char* skip_spaces(char* s)
{
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static void trim_tail(char* s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[len - 1] = '\0';
        len--;
    }
}

static bool streq(const char* a, const char* b)
{
    return strcmp(a, b) == 0;
}

static int parse_int(const char* s)
{
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

static bool parse_line(char* line, char** argv, int* argc, bool* bg)
{
    trim_newline(line);
    trim_tail(line);

    *argc = 0;
    *bg = false;

    char* cmd = skip_spaces(line);
    if (!cmd[0]) {
        return false;
    }

    int len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '&') {
        *bg = true;
        cmd[len - 1] = '\0';
        trim_tail(cmd);
    }

    while (*cmd && *argc < SHELL_MAX_ARGS - 1) {
        argv[*argc] = cmd;
        (*argc)++;
        while (*cmd && *cmd != ' ' && *cmd != '\t') {
            cmd++;
        }
        if (!*cmd) {
            break;
        }
        *cmd = '\0';
        cmd = skip_spaces(cmd + 1);
    }
    argv[*argc] = nullptr;
    return *argc > 0;
}

static bool has_slash(const char* cmd)
{
    for (int i = 0; cmd[i]; i++) {
        if (cmd[i] == '/') {
            return true;
        }
    }
    return false;
}

static void join_path(const char* dir, const char* cmd, char* out)
{
    int pos = 0;
    int i = 0;
    while (dir[i] && pos < SHELL_PATH_MAX - 1) {
        out[pos++] = dir[i++];
    }
    if (pos > 0 && out[pos - 1] != '/' && pos < SHELL_PATH_MAX - 1) {
        out[pos++] = '/';
    }
    i = 0;
    while (cmd[i] && pos < SHELL_PATH_MAX - 1) {
        out[pos++] = cmd[i];
        i++;
    }
    out[pos] = '\0';
}

static int exec_command(char** argv, int argc)
{
    static const char* path_dirs[SHELL_PATH_DIRS] = {"/bin", "/usr/bin"};
    char path[SHELL_PATH_MAX];
    const char* cmd = argv[0];
    (void)argc;

    if (has_slash(cmd)) {
        return execve(cmd, argv);
    }

    for (int i = 0; i < SHELL_PATH_DIRS; i++) {
        join_path(path_dirs[i], cmd, path);
        int ret = execve(path, argv);
        if (ret != -1) {
            return ret;
        }
    }
    return -1;
}

static Job* parse_job_arg(const char* arg)
{
    if (!arg || !arg[0]) {
        return nullptr;
    }
    if (arg[0] == '%') {
        return get_job_jid(parse_int(arg + 1));
    }
    return get_job_pid(parse_int(arg));
}

static void wait_foreground(Job* job)
{
    if (!job) {
        return;
    }

    int retval = 0;
    int pid = job->pid;
    job->state = JOB_FG;
    int waited = waitpid(pid, &retval);
    if (waited < 0) {
        printf("[shell] wait failed: %d\n", pid);
        delete_job(pid);
        return;
    }
    printf("[shell] %s exit %d\n", job->cmdline, retval);
    delete_job(pid);
}

static void do_bgfg(char** argv, int argc)
{
    if (argc < 2) {
        printf("[shell] %s requires PID or %%jobid\n", argv[0]);
        return;
    }

    Job* job = parse_job_arg(argv[1]);
    if (!job) {
        printf("[shell] no such job: %s\n", argv[1]);
        return;
    }

    if (streq(argv[0], "fg")) {
        wait_foreground(job);
        return;
    }

    job->state = JOB_BG;
    printf("[%d] (%d) %s\n", job->jid, job->pid, job->cmdline);
}

static bool builtin_cmd(char** argv, int argc)
{
    if (streq(argv[0], "pwd")) {
        char cwd[SHELL_PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) >= 0) {
            printf("%s\n", cwd);
        } else {
            printf("[shell] getcwd failed\n");
        }
        return true;
    }

    if (streq(argv[0], "cd")) {
        const char* path = argc >= 2 ? argv[1] : "/";
        if (chdir(path) != 0) {
            printf("[shell] cd failed: %s\n", path);
        }
        return true;
    }

    if (streq(argv[0], "jobs")) {
        list_jobs();
        return true;
    }

    if (streq(argv[0], "fg") || streq(argv[0], "bg")) {
        do_bgfg(argv, argc);
        return true;
    }

    if (streq(argv[0], "exit") || streq(argv[0], "quit")) {
        sync();
        exit(0);
    }

    if (streq(argv[0], "&")) {
        return true;
    }

    return false;
}

static void run_program(char** argv, int argc, bool bg, const char* cmdline)
{
    int pid = fork();
    if (pid < 0) {
        printf("[shell] fork failed\n");
        return;
    }

    if (pid == 0) {
        int ret = exec_command(argv, argc);
        if (has_slash(argv[0])) {
            printf("[shell] command not found: %s (%d)\n", argv[0], ret);
        } else {
            printf("[shell] command not found in PATH: %s (%d)\n", argv[0], ret);
        }
        return;
    }

    if (!add_job(pid, bg ? JOB_BG : JOB_FG, cmdline)) {
        int retval = 0;
        waitpid(pid, &retval);
        return;
    }

    Job* job = get_job_pid(pid);
    if (bg) {
        printf("[%d] (%d) %s\n", job->jid, pid, job->cmdline);
        return;
    }

    wait_foreground(job);
}

static void print_prompt()
{
    write("osh> ");
}

int main(int boot_argc, char** boot_argv)
{
    (void)boot_argc;
    (void)boot_argv;
    char line[SHELL_LINE_MAX];
    char original[SHELL_LINE_MAX];
    char* argv[SHELL_MAX_ARGS];
    int argc = 0;
    bool bg = false;

    init_jobs();
    printf("[shell] ready\n");
    printf("[shell] PATH=/bin:/usr/bin\n");
    printf("[shell] use /path/name for explicit paths\n");

    while (1) {
        memset(line, 0, sizeof(line));
        memset(original, 0, sizeof(original));
        print_prompt();

        int n = read(0, line, sizeof(line) - 1);
        if (n <= 0) {
            printf("\n[shell] read failed: %d\n", n);
            continue;
        }
        trim_newline(line);
        strcpy(original, line);

        if (!parse_line(line, argv, &argc, &bg)) {
            continue;
        }

        if (builtin_cmd(argv, argc)) {
            continue;
        }

        run_program(argv, argc, bg, original);
    }

    return 0;
}
